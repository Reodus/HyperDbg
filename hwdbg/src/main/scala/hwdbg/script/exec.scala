/**
 * @file
 *   exec.scala
 * @author
 *   Sina Karvandi (sina@hyperdbg.org)
 * @brief
 *   Script execution engine
 * @details
 * @version 0.1
 * @date
 *   2024-05-07
 *
 * @copyright
 *   This project is released under the GNU Public License v3.
 */
package hwdbg.script

import chisel3._
import chisel3.util._

import hwdbg.configs._
import hwdbg.stage._

object ScriptExecutionEngineConfigStage {
  object State extends ChiselEnum {
    val sConfigStageSymbol, sConfigGetSymbol, sConfigSetSymbol = Value
  }
}

class ScriptExecutionEngine(
    debug: Boolean = DebuggerConfigurations.ENABLE_DEBUG,
    instanceInfo: HwdbgInstanceInformation
) extends Module {

  //
  // Import state enum
  //
  import ScriptExecutionEngineConfigStage.State
  import ScriptExecutionEngineConfigStage.State._

  val io = IO(new Bundle {

    //
    // Chip signals
    //
    val en = Input(Bool()) // chip enable signal

    //
    // Script stage configuration signals
    //
    val finishedScriptConfiguration = Input(Bool()) // whether script configuration finished or not?
    val configureStage = Input(Bool()) // whether the configuration of stage should start or not?
    val targetOperator = Input(new HwdbgShortSymbol(instanceInfo.scriptVariableLength)) // Current operator to be configured

    //
    // Input/Output signals
    //
    val inputPin = Input(Vec(instanceInfo.numberOfPins, UInt(1.W))) // input pins
    val outputPin = Output(Vec(instanceInfo.numberOfPins, UInt(1.W))) // output pins
  })

  //
  // Output pins
  //
  val outputPin = Wire(Vec(instanceInfo.numberOfPins, UInt(1.W)))

  //
  // Stage registers
  //
  val stageRegs = Reg(Vec(instanceInfo.maximumNumberOfStages, new Stage(debug, instanceInfo)))

  //
  // Stage configuration registers
  //
  val configState = RegInit(sConfigStageSymbol)
  val configStageNumber = RegInit(1.U(log2Ceil(instanceInfo.maximumNumberOfStages).W)) // reset to one because the first stage is used for saving data

  //
  // Calculate the maximum of the two values since we only want to use one register for 
  // both GET and SET
  //
  val maxOperators = math.max(instanceInfo.maximumNumberOfSupportedGetScriptOperators, instanceInfo.maximumNumberOfSupportedSetScriptOperators)

  //
  // Create a register with the width based on the maximum value
  //
  val configGetSetOperatorNumber = RegInit(0.U(log2Ceil(maxOperators).W)) 

  // -----------------------------------------------------------------------
  //
  // *** Configure stage buffers ***
  //
  when (io.configureStage === true.B) {

    switch(configState) {

      is(sConfigStageSymbol) {
        
        //
        // Configure the stage symbol (The first symbol is the stage operator)
        //
        stageRegs(configStageNumber).stageSymbol := io.targetOperator
        configState := sConfigGetSymbol

        //
        // If it is the very first configuration symbol, then we disable all stages
        //
        when (configStageNumber === 1.U) {
          for (i <- 0 until instanceInfo.maximumNumberOfStages) {
              stageRegs(i).stageEnable := false.B
          }
        }
      }
      is(sConfigGetSymbol) {

        //
        // Config GET operator
        //
        stageRegs(configStageNumber).getOperatorSymbol(configGetSetOperatorNumber) := io.targetOperator
        configGetSetOperatorNumber := configGetSetOperatorNumber + 1.U 

        when(configGetSetOperatorNumber === (instanceInfo.maximumNumberOfSupportedGetScriptOperators - 1).U) {

          configGetSetOperatorNumber := 0.U // reset the counter
          configState := sConfigSetSymbol // go to the next state
        }
      }
      is(sConfigSetSymbol) {
        //
        // Config SET operator
        //
        stageRegs(configStageNumber).setOperatorSymbol(configGetSetOperatorNumber) := io.targetOperator
        stageRegs(configStageNumber).stageEnable := true.B // this stage is enabled
        configGetSetOperatorNumber := configGetSetOperatorNumber + 1.U 

        when(configGetSetOperatorNumber === (instanceInfo.maximumNumberOfSupportedSetScriptOperators - 1).U) {

          configGetSetOperatorNumber := 0.U // reset the counter

          when (io.finishedScriptConfiguration === true.B) {
            //
            // Not configuring anymore, reset the stage number
            //
            configStageNumber := 1.U // reset to one because the first stage is used for saving data
            configState := sConfigStageSymbol
          }.otherwise {
            configStageNumber := configStageNumber + 1.U // Increment the stage number holder of current configuration
            configState := sConfigStageSymbol // the next state is again a stage symbol
          }
        }
      }
    }
  }

  // -----------------------------------------------------------------------
  //
  // *** Move each register (input vector) to the next stage at each clock ***
  //
  for (i <- 0 until instanceInfo.maximumNumberOfStages) {

    if (i == 0) {

      //
      // At the first stage, the input registers should be passed to the
      // first registers set of the stage registers
      //
      stageRegs(0).pinValues := io.inputPin

      //
      // Each pin start initially start from 0th target stage
      //
      stageRegs(0).targetStage := 0.U

    } else if (i == (instanceInfo.maximumNumberOfStages - 1)) {

      //
      // At the last stage, the state registers should be passed to the output
      // Note: At this stage script symbol is useless
      //
      outputPin := stageRegs(i - 1).pinValues

    } else {

      //
      // Check if this stage should be ignored (passed to the next stage) or be evaluated
      // (i - 1) is because the 0th index registers are used for storing data but the 
      // script engine assumes that the symbols start from 0, so -1 is used here 
      //
      when((i - 1).U === stageRegs(i - 1).targetStage && stageRegs(i).stageEnable === true.B) {

        //
        // *** Based on target stage, this stage needs evaluation ***
        //
        
        //
        // Instantiate an eval engine for this stage
        //
        val (
          nextStage,
          outputPin
        ) = ScriptEngineEval(
          debug,
          instanceInfo
        )(
          stageRegs(i - 1).stageEnable,
          stageRegs(i - 1)
        )

        //
        // At the normal (middle) stage, the result of state registers should be passed to
        // the next level of stage registers
        //
        stageRegs(i).pinValues := outputPin

        //
        // Pass the target stage symbol number to the next stage
        //
        stageRegs(i).targetStage := nextStage

      }.otherwise {

        //
        // *** Based on target stage, this stage should be ignore ***
        //

        //
        // Just pass all the values to the next stage
        //
        stageRegs(i).pinValues := stageRegs(i).pinValues
        stageRegs(i).targetStage := stageRegs(i).targetStage

      }
    }
  }

  // -----------------------------------------------------------------------

  //
  // Connect the output signals
  //
  io.outputPin := outputPin

}

object ScriptExecutionEngine {

  def apply(
      debug: Boolean = DebuggerConfigurations.ENABLE_DEBUG,
      instanceInfo: HwdbgInstanceInformation
  )(
      en: Bool,
      finishedScriptConfiguration: Bool,
      configureStage: Bool,
      targetOperator: HwdbgShortSymbol,
      inputPin: Vec[UInt]
  ): (Vec[UInt]) = {

    val scriptExecutionEngineModule = Module(
      new ScriptExecutionEngine(
        debug,
        instanceInfo
      )
    )

    val outputPin = Wire(Vec(instanceInfo.numberOfPins, UInt(1.W)))

    //
    // Configure the input signals
    //
    scriptExecutionEngineModule.io.en := en
    scriptExecutionEngineModule.io.finishedScriptConfiguration := finishedScriptConfiguration
    scriptExecutionEngineModule.io.configureStage := configureStage
    scriptExecutionEngineModule.io.targetOperator := targetOperator
    scriptExecutionEngineModule.io.inputPin := inputPin

    //
    // Configure the output signals
    //
    outputPin := scriptExecutionEngineModule.io.outputPin

    //
    // Return the output result
    //
    (outputPin)
  }
}
