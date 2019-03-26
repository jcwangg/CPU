//
//  sim4.c
//  Author: Joyce Wang
//
//  This file contains a series of functions that together implement a single cycle CPU. This CPU supports the instructions: add, addu, sub, subu, addi, addiu, and, or, xor, slt, slti, lw, sw, beq, j, and the "extra"  instructions andi, sll, and bne.
//
//  Created by Joyce Wang on 11/6/18.
//

#include "sim4.h"

/*
 This function uses the current program counter to read the proper word out of instruction memory and returns it.
 */
WORD getInstruction(WORD curPC, WORD *instructionMemory){
    //convert bytes to proper array location
    int arrIndex = curPC/4;
    return instructionMemory[arrIndex];
}

/*
 This function extracts all the fields out of a given instruction and stores the fields into the InstructionFields struct. This incudes opcode, rs, rt, rd, shamt, funct, imm16, imm32, and address.
 */
void extract_instructionFields(WORD instruction, InstructionFields *fieldsOut){
    //6 bits
    fieldsOut->opcode = (instruction >> 26) & 0x3f;
    //5 bits each
    fieldsOut->rs = (instruction >> 21) & 0x1f;
    fieldsOut->rt = (instruction >> 16) & 0x1f;
    fieldsOut->rd = (instruction >> 11) & 0x1f;
    fieldsOut->shamt = (instruction >> 6) & 0x1f;
    //6 bits
    fieldsOut->funct = (instruction) & 0x3f;
    //16 bit immediate value
    fieldsOut->imm16 = (instruction) & 0xffff;
    fieldsOut->imm32 = signExtend16to32((instruction) & 0xffff);
    //28 bit address field
    fieldsOut->address = (instruction) & 0x3ffffff;
}

/*
 This function takes in the InstructionFields and reads the opcode and funct to set the control fields in the CPUControl struct. This includes extra1 set to 1 if it is a shift instruction. It returns 0 if the instruction is invalid and 1 if it is recognized.
 */
int  fill_CPUControl(InstructionFields *fields, CPUControl *controlOut){
    int op = fields->opcode;
    int func = fields->funct;
    //extra1 == 1 if shift instruction
    if (op==0 && func==0){
        controlOut->extra1 = 1;
    }
    else{
        controlOut->extra1 = 0;
    }
    //extra2 and extra3 are unused
    controlOut->extra2 = 0;
    controlOut->extra3 = 0;
    //check for invalid opcode or funct
    if (op!=0 && op!=2 && op!=4 && op!=5 && op!=8 && op!=9 && op!=10 && op!=12 && op!=35 && op!=43){
        return 0;
    }
    else if (op ==0){
        if ((func<32 && func!=0) || (func>38 && func!= 42)){
            return 0;
        }
    }
    //set ALUsrc
    if (op>5){
      controlOut->ALUsrc = 1;
    }
    else{
        controlOut->ALUsrc = 0;
    }
    //set ALUop
    if ((op==0 && func == 42)|| op ==10){
        controlOut->ALU.op = 3;
    }
    else if ((op==0 && func == 36) || op ==2 || op==12 || (op==0 && func ==0)){
        controlOut->ALU.op = 0;
    }
    else if (op==0 && func == 37){
        controlOut->ALU.op = 1;
    }
    else if (op==0 && func == 38){
        controlOut->ALU.op = 4;
    }
    else {
        controlOut->ALU.op = 2;
    }
    //set bNegate
    if (op==4 || op==5 || op ==10 || (op==0 &&(func ==34 || func == 35 ||func == 42))){
        controlOut->ALU.bNegate = 1;
        //set branch
        if (op ==4 || op==5){
            controlOut->branch = 1;
        }
    }
    else {
        controlOut->ALU.bNegate = 0;
        controlOut->branch = 0;
    }
    //set memRead and memtoReg
    if (op ==35){
        controlOut->memRead = 1;
        controlOut->memToReg = 1;
    }
    else{
        controlOut->memRead = 0;
        controlOut->memToReg = 0;
    }
    //set memWrite
    if (op ==43){
        controlOut->memWrite = 1;
    }
    else{
        controlOut->memWrite = 0;
    }
    //set jump
    if (op ==2){
        controlOut->jump = 1;
    }
    else{
        controlOut->jump = 0;
    }
    //set regDst and regWrite
    if (op == 0){
        controlOut->regDst = 1;
        controlOut->regWrite = 1;
    }
    else{
        controlOut->regDst = 0;
        controlOut->regWrite = 0;
    }
    //set regWrite
    if (op == 8 || op == 9 || op == 10 || op == 35 || op ==12){
        controlOut->regWrite = 1;
    }
    //indicates valid opcode
    return 1;
}

/*
 This function returns ALU input 1. If shift, it returns rtVal. Otherwise, return rsVal.
 */
WORD getALUinput1(CPUControl *controlIn,
                  InstructionFields *fieldsIn,
                  WORD rsVal, WORD rtVal, WORD reg32, WORD reg33,
                  WORD oldPC){
    //if shift
    if (controlIn->extra1 == 1){
        return rtVal;
    }
    return rsVal;
}

/*
 This function returns ALU input 2 based on the control fields and what type of instruction it is.
 */
WORD getALUinput2(CPUControl *controlIn,
                  InstructionFields *fieldsIn,
                  WORD rsVal, WORD rtVal, WORD reg32, WORD reg33,
                  WORD oldPC){
    //if shift
    if (controlIn->extra1 == 1){
        return fieldsIn->shamt;
    }
    else if (controlIn->regDst == 1 || controlIn->branch == 1 || controlIn->jump == 1){
        return rtVal;
    }
    else if (controlIn->regWrite == 0 && fieldsIn->opcode != 43){
        return 0;
    }
    //andi is zero extended not sign extended
    else if(fieldsIn->opcode == 12){
        return fieldsIn->imm16;
    }
    return fieldsIn->imm32;
}

/*
 This function implements the ALU and sets the fields of ALUResultOut, which are result, extra, and zero. It uses the control fields and input1 and input2. If the result is zero, zero is set to 1.
 */
void execute_ALU(CPUControl *controlIn,
                 WORD input1, WORD input2,
                 ALUResult  *aluResultOut){
    //AND
    if (controlIn->ALU.op == 0){
        //jump instruction
        if (controlIn->jump == 1){
            aluResultOut->result = input1 & input2;
            aluResultOut->extra = 0;
        }
        //shift instruction
        else if (controlIn->extra1==1){
            aluResultOut->result = input1<<input2;
            aluResultOut->extra = 0;
        }
        else{
            aluResultOut->result = input1 & input2;
            aluResultOut->extra =0;
        }
    }
    //OR
    else if (controlIn->ALU.op == 1){
        aluResultOut->result = input1 | input2;
        aluResultOut->extra = 0;
        aluResultOut->zero = 0;
    }
    //XOR
    else if (controlIn->ALU.op == 4){
        aluResultOut->result = input1 ^ input2;
        aluResultOut->extra = 0;
        aluResultOut->zero = 0;
    }
    //ADD
    else if (controlIn->ALU.op == 2){
        if (controlIn->ALU.bNegate == 0){
            aluResultOut->result = input1 + input2;
            aluResultOut->extra = 0;
        }
        //SUBTRACT
        else {
            aluResultOut->result = input1 - input2;
            aluResultOut->extra = 0;
        }
    }
    //LESS (result is 1 if 1-2 is negative)
    else if (controlIn->ALU.op == 3){
        if (input1-input2<0){
            aluResultOut->result = 1;
            aluResultOut->extra = 0;
            aluResultOut->zero = 0;
        }
        else {
            aluResultOut->result = 0;
            aluResultOut->extra = 0;
            aluResultOut->zero = 1;
        }
        return;
    }
    //zero indicates if result is zero
    if (aluResultOut->result == 0){
        aluResultOut->zero =1;
    }
}

/*
 This function implements the data memory unit and sets the MemResult fields using the control fields. If memory is not accessed, the readVal field of result is set to zero.
 */
void execute_MEM(CPUControl *controlIn,
                 ALUResult  *aluResultIn,
                 WORD        rsVal, WORD rtVal,
                 WORD       *memory,
                 MemResult  *resultOut){
    //load word
    if (controlIn->memRead == 1){
        resultOut->readVal = memory[aluResultIn->result/4];
    }
    //store word
    else if (controlIn->memWrite == 1){
        resultOut->readVal = 0;
        memory[aluResultIn->result/4]=rtVal; 
    }
    else{
        resultOut->readVal = 0; 
    }
}

/*
 This function returns the next program counter based on whether it is a jump or branch instruction, or other kind of instruction.
 */
WORD getNextPC(InstructionFields *fields, CPUControl *controlIn, int aluZero,
               WORD rsVal, WORD rtVal,
               WORD oldPC){
    //if equal, branch to address
    if (fields->opcode == 4 && controlIn->branch == 1 && aluZero ==1){
        return oldPC+4+(fields->imm16<<2);
    }
    //if not equal, branch to address
    if (fields->opcode == 5 && controlIn->branch == 1 && aluZero ==0){
        return oldPC+4+(fields->imm16<<2);
    }
    //jump to address
    if (controlIn->jump == 1){
        return (oldPC & (0xf0000000)) | (fields->address<<2);
    }
    return oldPC+4;
}

/*
 This function writes to a register if needed.It updates the appropriate register in the current set of registers, regs.
 */
void execute_updateRegs(InstructionFields *fields, CPUControl *controlIn,
                        ALUResult  *aluResultIn, MemResult *memResultIn,
                        WORD       *regs){
    //load word
    if (controlIn->memToReg == 1){
        regs[fields->rt]= memResultIn->readVal;
    }
    //j format, beq, bne do not update registers
    else if (fields->opcode == 2 || fields->opcode == 4 ||fields->opcode == 5) {
    }
    //i format use rt as destination
    else if (controlIn->regDst == 0 && controlIn->regWrite==1){
        regs[fields->rt] = aluResultIn->result;
    }
    //r format instructions use destination register
    else if (controlIn->regWrite ==1){
        regs[fields->rd] = aluResultIn->result;
    }
}



