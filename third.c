#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_REG 32
#define MAX_MEM 16 * 1024 * 1024 / sizeof(int)

#define sp 29
#define ra 31

#define add 0x20
#define addu 0x21
#define and 0x24
#define jr 0x8
#define nor 0x27
#define or 0x25
#define slt 0x2a
#define sltu 0x2b
#define sll 0x0
#define srl 0x2
#define sub 0x22
#define subu 0x23

#define addi 0x8
#define addiu 0x9
#define andi 0xc
#define beq 0x4
#define bne 0x5
#define j 0x2
#define jal 0x3
#define lui 0xf
#define lw 0x23
#define ori 0xd
#define slti 0xa
#define sltiu 0xb
#define sw 0x2b

int Mem[MAX_MEM];
int regfile[MAX_REG];
int pc;
int statClockCycle;
int statInst, statMem, statReg, statExec, statBranch, statBranchNotT;
int statJump=0;

struct ifId{
    int inst;
    int pc;
} ifId;
struct ifId ifIdLatch[2];

struct idEx {
    int inst;
    int pc;
    int opcode, rs, rt, rd, shamt, funct, imm, address;
    int vRs, vRt;
    int JumAddr, branchaddr;
    int signextimm, zeroextimm;
    int MemWrite, MemtoReg, RegDst, MemRead, RegWrite;
    int branch, Zero, PCSrc;
} idEx;
struct idEx idExLatch[2];

struct exMem {
    int inst;
    int pc;
    int opcode, funct, signextimm;
    int vRs, vRt, reg;
    int alu;
    int branch, Zero, PCSrc;
    int MemWrite, MemtoReg, MemRead, RegWrite;
    int index;
} exMem;
struct exMem exMemLatch[2];

struct memWb{
    int inst;
    int pc;
    int opcode, funct;
    int vRs, vRt, reg;
    int Memresult, alu;
    int MemtoReg, RegWrite;
    int index;
} memWb;
struct memWb memWbLatch[2];

void flushlatchIfIdEx(){
    idExLatch[0].pc = ifIdLatch[1].pc;
    idExLatch[0].inst = ifIdLatch[1].inst;
}

void flushlatchIdExMem(){
    exMemLatch[0].pc = idExLatch[1].pc;
    exMemLatch[0].inst = idExLatch[1].inst;
    exMemLatch[0].opcode = idExLatch[1].opcode;
    exMemLatch[0].funct = idExLatch[1].funct;
    exMemLatch[0].vRt = idExLatch[1].vRt;
    exMemLatch[0].branch = idExLatch[1].branch;
    exMemLatch[0].PCSrc = idExLatch[1].PCSrc;
    exMemLatch[0].MemWrite = idExLatch[1].MemWrite;
    exMemLatch[0].MemRead = idExLatch[1].MemRead;
    exMemLatch[0].MemtoReg = idExLatch[1].MemtoReg;
    exMemLatch[0].RegWrite = idExLatch[1].RegWrite;
}

void flushlatchExMemWb(){
    memWbLatch[0].alu = exMemLatch[1].alu;
    memWbLatch[0].inst = exMemLatch[1].inst;
    memWbLatch[0].vRs = exMemLatch[1].vRs;
    memWbLatch[0].vRt = exMemLatch[1].vRt;
    memWbLatch[0].reg = exMemLatch[1].reg;
    memWbLatch[0].opcode = exMemLatch[1].opcode;
    memWbLatch[0].funct = exMemLatch[1].funct;
    memWbLatch[0].MemtoReg = exMemLatch[1].MemtoReg;
    memWbLatch[0].RegWrite = exMemLatch[1].RegWrite;
}

void initialize() { 
    bzero(regfile, sizeof(regfile)); 
    pc = 0;
    //regfile[sp] = (unsigned int *) 0x1000000;
    regfile[ra] = 0xFFFFFFFF;
} 

void fetch(){
    if (pc == 0xFFFFFFFF)
    return 0;
    
    ifIdLatch[0].inst = Mem[pc/4];
    ifIdLatch[0].pc = pc;
 
    printf("Fet: [0x%08X]: %08X\n", pc, ifIdLatch[0].inst);
    pc = pc +4;
}

void decode(){
    //opcode
    idExLatch[0].opcode = (ifIdLatch[1].inst & 0xFC000000) >> 26;
    
    idExLatch[0].rs = (ifIdLatch[1].inst & 0x03E00000);
    idExLatch[0].rs = idExLatch[0].rs >> 21; 

    idExLatch[0].rt = (ifIdLatch[1].inst & 0x001F0000);
    idExLatch[0].rt = idExLatch[0].rt >> 16;
    
    if (idExLatch[0].opcode == 0x0){ //r type
        idExLatch[0].rd = (ifIdLatch[1].inst & 0x0000F800);
        idExLatch[0].rd = idExLatch[0].rd >> 11;

        idExLatch[0].shamt = (ifIdLatch[1].inst & 0x000007C0);
        idExLatch[0].shamt = idExLatch[0].shamt >> 6;

        idExLatch[0].funct = (ifIdLatch[1].inst & 0x0000003F);

        idExLatch[0].vRs = regfile[idExLatch[0].rs];
        idExLatch[0].vRt = regfile[idExLatch[0].rt];

    printf("Dec:\t R type rs: %d, rt: %d, rd: %d, shamt: %d, funct: %d\n", idExLatch[0].rs, idExLatch[0].rt, idExLatch[0].rd, idExLatch[0].shamt, idExLatch[0].funct);
    } else if (idExLatch[0].opcode == 0x2 || idExLatch[0].opcode == 0x3) { //j type
        idExLatch[0].address = (ifIdLatch[1].inst & 0x3ffffff);
        printf("Dec: \t J type: %d\n", idExLatch[0].address);
    } else { //i type 
        idExLatch[0].imm = (ifIdLatch[1].inst & 0xffff);

        idExLatch[0].vRs = regfile[idExLatch[0].rs];
        idExLatch[0].vRt = regfile[idExLatch[0].rt];

        if ((ifIdLatch[1].inst & 0x0000ffff) >= 0x8000){
            idExLatch[0].signextimm = 0xffff0000 | (ifIdLatch[1].inst & 0x0000ffff);
        } else {
            idExLatch[0].signextimm = (ifIdLatch[1].inst & 0x0000ffff);
        }
        printf("Dec:\t rs: %d, rt: %d, imm: %d, Simm:%d\n", idExLatch[0].rs, idExLatch[0].rt, idExLatch[0].imm, idExLatch[0].signextimm);
    }
    printf("OPCODE: 0x%x\n", idExLatch[0].opcode);
    idExLatch[0].JumAddr =ifIdLatch[1].pc & 0xf0000000 | idExLatch[0].address << 2;

    flushlatchIfIdEx();

    if(idExLatch[0].opcode == j) { //0x2
        pc = idExLatch[0].JumAddr;
        statJump++;
        statExec++;
        printf("JUMP: %d \n", idExLatch[0].JumAddr);
    } else if (idExLatch[0].opcode == jal){ //0x3
        regfile[ra] = idExLatch[0].pc + 8;
        pc = idExLatch[0].JumAddr;
        statJump++;
        statExec++;
        printf("JAL: %d \n", idExLatch[0].JumAddr);
    }

    if(idExLatch[0].opcode == 0x0){
        idExLatch[0].RegDst = 1;
    } else {
        idExLatch[0].RegDst = 0;
    }

    if ((idExLatch[0].opcode == beq) || (idExLatch[0].opcode == bne)){ //0x4    0x5
        idExLatch[0].branch = 1;
    } else {
        idExLatch[0].branch = 0;
    }

    if (idExLatch[0].opcode == lw){ //0x23
        idExLatch[0].MemtoReg = 1;
    } else {
        idExLatch[0].MemtoReg = 0;
    }

    if (idExLatch[0].opcode == lw){ //0x23
        idExLatch[0].MemRead = 1;
    } else {
        idExLatch[0].MemRead = 0;
    }

    if (idExLatch[0].opcode == sw){ // 0x2b
        idExLatch[0].MemWrite = 1;
    } else {
        idExLatch[0].MemWrite = 0;
    }
 
    //0x8: jr       0x4: beq        0x5: bne      0x2: j      0x3: jal      0x2b: sw                
    if ((idExLatch[0].funct != jr) && (idExLatch[0].opcode != beq) && (idExLatch[0].opcode != bne) && (idExLatch[0].opcode != j) && (idExLatch[0].opcode != jal)&& (idExLatch[0].opcode != sw)){
        idExLatch[0].RegWrite = 1;
    } else {
        idExLatch[0].RegWrite = 0;
    }

}

void execute() {
    int vRs, vRt;

    idExLatch[0].vRs = regfile[idExLatch[0].rs];
    idExLatch[0].vRt = regfile[idExLatch[0].rt];

    //idExLatch[1].JumAddr = ifIdLatch[1].pc & 0xF0000000 | idExLatch[0].address << 2;
    idExLatch[1].branchaddr = idExLatch[1].signextimm << 2;
    idExLatch[1].zeroextimm = (idExLatch[1].inst & 0x0000FFFF);

    if ((idExLatch[1].rs != 0) && (idExLatch[1].rs == exMemLatch[0].reg) && (exMemLatch[0].RegWrite)){
        idExLatch[1].vRs = exMemLatch[0].alu;
        printf("DEPENDENCY RS #1 \n");
    } else if ((idExLatch[1].rs != 0) && (idExLatch[1].rs == memWbLatch[0].reg) && (memWbLatch[0].RegWrite)) {
        if (memWbLatch[0].MemtoReg == 1){
            idExLatch[1].vRs = memWbLatch[0].Memresult;
            printf("DEPENDENCY RS DIST #2 (LW)\n");
        } else {
            idExLatch[1].vRs = memWbLatch[0].alu;
            printf("DEPENDENCY RS DIST #2\n");
        }
    }

    if ((idExLatch[1].rt != 0) && (idExLatch[1].rt == exMemLatch[0].reg) && (exMemLatch[0].RegWrite)){
        idExLatch[1].vRt = exMemLatch[0].alu;
        printf("DEPENDENCY RT DIST #1\n");
    } else if ((idExLatch[1].rt != 0) && (idExLatch[1].rt == memWbLatch[0].reg) && (memWbLatch[0].RegWrite)){
        if (memWbLatch[0].MemtoReg == 1){
            idExLatch[1].vRt == memWbLatch[0].Memresult;
            printf("DEPENDENCY RT DIST #2 (LW)\n");
        } else {
            idExLatch[1].vRt = memWbLatch[0].alu;
            printf("DEPENDENCY RT DIST #2\n");
        }
    }

    if (idExLatch[1].opcode == 0) {
        if (idExLatch[1].funct == add){ //0x20
            exMemLatch[0].alu = idExLatch[1].vRs + idExLatch[1].vRt; // bs ganti vRs = idExLatch[1].vRs; lgsg kek gitu
            statExec++;
            printf("EXE: ADD\n");
        }else if (idExLatch[1].funct == addu) { //0x21
            exMemLatch[0].alu = idExLatch[1].vRs + idExLatch[1].vRt;
            statExec++;
            printf("EXE: ADDU\n");
        } else if (idExLatch[1].funct == and) { //0x24
            exMemLatch[0].alu = idExLatch[1].vRs & idExLatch[1].vRt;
            statExec++;
            printf("EXE: AND\n");
        }else if (idExLatch[1].funct == jr){ //0x8
            pc = idExLatch[1].vRs;
            printf("EXE: JR \t PC = %d\n", pc);
            statJump++;
        } else if (idExLatch[1].funct == nor) { //0x27
            exMemLatch[0].alu = ~(idExLatch[1].vRs | idExLatch[1].vRt);
            statExec++;
            printf("EXE: NOR\n");
        }else if (idExLatch[1].funct == or){ //0x25
            exMemLatch[0].alu = idExLatch[1].vRs | idExLatch[1].vRt;
            statExec++;
            printf("EXE: OR\n");
        }else if (idExLatch[1].funct == slt) { //0x2a
            exMemLatch[0].alu = (idExLatch[1].vRs < idExLatch[1].vRt)? 1 : 0;
            statExec++;
           printf("EXE: SLT\n");
        }else if (idExLatch[1].funct == sltu) { //0x2b
            exMemLatch[0].alu = (idExLatch[1].vRs < idExLatch[1].vRt)? 1 : 0;
            statExec++;
            printf("EXE: SLTU\n");
        }else if (idExLatch[1].funct == sll ) { //0x0
            exMemLatch[0].alu = idExLatch[1].vRt << idExLatch[1].shamt;
            if (idExLatch[1].inst == 0x00000000){
                statExec++;
                printf("EXE: NOP\n");
            } else {
                statExec++;
                printf("EXE: SLL\n");
            }
        } else if (idExLatch[1].funct == srl) { //0x2
            exMemLatch[0].alu = idExLatch[1].vRt >> idExLatch[1].shamt;
            statExec++;
            printf("EXE: SRL\n");
        } else if (idExLatch[1].funct == sub){ //0x22
            exMemLatch[0].alu = idExLatch[1].vRs - idExLatch[1].vRt;
            statExec++;
            printf("EXE: SUB\n");
        } else if (idExLatch[1].funct == subu){ //0x23
            exMemLatch[0].alu = idExLatch[1].vRs + idExLatch[1].vRt;
            statExec++;
            printf("EXE: SUBU\n");
        }

    } else if (idExLatch[1].opcode == addi){ //0x8
        exMemLatch[0].alu = idExLatch[1].vRs + idExLatch[1].signextimm;
        statExec++;
        printf("EXE: ADDI\n");
    }else if (idExLatch[1].opcode == addiu) { //0x9
        exMemLatch[0].alu = idExLatch[1].vRs + idExLatch[1].signextimm;
        statExec++;
        printf("EXE: ADDIU\n");
    }else if (idExLatch[1].opcode == andi) { //0xc
        exMemLatch[0].alu = idExLatch[1].vRs & idExLatch[1].zeroextimm;
        statExec++;
        printf("EXE: ANDI\n");
    }else if (idExLatch[1].opcode == beq) { //0x4
        if (vRs == vRt){
            pc = idExLatch[1].pc + 4 + idExLatch[1].branchaddr;
            memset(&ifIdLatch[0], 0, sizeof(ifId));
            memset(&idExLatch[0], 0, sizeof(idEx));
            statBranch++;
            statExec++;
            printf("EXE: BEQ\n");
        } else {
            statBranchNotT++;
            printf("EXE: BEQ\n");
        }
    }else if (idExLatch[1].opcode == bne) { //0x5
        if (vRs != vRt){
            pc = idExLatch[1].pc + 4 + idExLatch[1].branchaddr;
            memset(&ifIdLatch[0], 0, sizeof(ifId));
            memset(&idExLatch[0], 0, sizeof(idEx));
            statBranch++;
            statExec++;
            printf("EXE: BNE\n");
        } else {
            statBranchNotT++;
            printf("EXE: BNE\n");
        }
    // }else if (idExLatch[1].opcode == j) { //0x2
    //     pc = idExLatch[0].JumAddr;
    //     statJump++;;
    //     statExec++;
    //     printf("EXE: JUMP\n");
    // }else if (idExLatch[1].opcode == jal) { //0x3
    //     regfile[ra] = idExLatch[0].pc + 8;
    //     pc = idExLatch[0].JumAddr;
    //     statJump++;
    //     statExec++;
    //     printf("EXE: JAL\n");
    
    } else if (idExLatch[1].opcode == lui) { //0xf
        exMemLatch[1].alu = idExLatch[1].imm << 16;
        statExec++;
        printf("EXE: LUI\n");
    }else if (idExLatch[1].opcode == lw) { //0x23
        exMemLatch[0].alu = idExLatch[1].vRs + idExLatch[1].signextimm;
        statExec++;
        printf("EXE: LW\n");
    }else if (idExLatch[1].opcode == ori){ //0xd
        exMemLatch[0].alu = idExLatch[1].vRs | idExLatch[1].zeroextimm;
        statExec++;
        printf("EXE: ORI\n");
    }else if (idExLatch[1].opcode == slti){ //0xa
        exMemLatch[0].alu = ((idExLatch[1].vRs < idExLatch[1].signextimm) ? 1 : 0);
        statExec++;
        printf("EXE: SLTI\n");
    }else if (idExLatch[1].opcode == sltiu){ //0xb
        exMemLatch[0].alu = ((idExLatch[1].vRs < idExLatch[1].signextimm) ? 1 : 0);
        statExec++;
        printf("EXE: SLTIU\n");
    }else if (idExLatch[1].opcode == sw){ //0x2b
        exMemLatch[0].alu = idExLatch[1].vRt;
        exMemLatch[0].index = idExLatch[1].vRs + idExLatch[1].signextimm;
        statExec++;
        printf("EXE: SW\n");
    }

    if (idExLatch[1].RegDst == 1){
        exMemLatch[0].reg = idExLatch[1].rd;
    } else {
        exMemLatch[0].reg = idExLatch[1].rt;
    }


    //printf("\n");
    flushlatchIdExMem();
}

void memoryaccess() {
    if ( exMemLatch[1].MemRead == 1) { //lw
        memWbLatch[0].Memresult = Mem[exMemLatch[1].alu];
        statMem++;
        printf("MEM: MEMREAD %d\n", exMemLatch[1].MemRead);
    }

    if (exMemLatch[1].MemWrite == 1) { //sw
        Mem[ exMemLatch[1].index] = exMemLatch[1].alu;
        statMem++;
        printf("MEM: MEMWRITE %d\n", exMemLatch[1].MemWrite);
    }

    flushlatchExMemWb();
    //printf("\n");

}

void writeback() {
    if (memWbLatch[1].RegWrite == 1) {
        if (memWbLatch[1].MemtoReg == 1){
            regfile[memWbLatch[1].reg] = memWbLatch[1].Memresult;
            printf("WB: MEMTOREG %d\n", memWbLatch[1].MemtoReg);
            statReg++;
        } else {
             regfile[memWbLatch[1].reg] = memWbLatch[1].alu;
            printf("WB: MEMTOREG %d\n", memWbLatch[1].RegWrite);
            statReg++;
        }
    } else {
        printf("WB: NO WB\n");
    }
    //printf("\n");
}

void main (int argc, char *argv[]){
    FILE *fd;
    int ret;
    char *filename;
    int mips_val, i=0;
    int mem_val;
    initialize();

    if (argc == 2){
        filename = argv[1];
    } else {
        filename = "file/name/path";
    }

    if (fd == NULL){
        printf("Exit\n");
        exit(1);
    }
    else {
        fd = fopen(filename, "rb");
    }

    do {
        mips_val = 0;
        ret = fread(&mips_val, 1, 4, fd);
        mem_val = ntohl(mips_val);
        mem_val = (mips_val & 0xFF) << 24 | (mips_val & 0xFF00) << 8 | (mips_val & 0xFF0000) >> 8 | (mips_val & 0xFF000000) >> 24;
        Mem[i] = mem_val;
        //printf("(%d) Load Mem[%X] pa: 0x%x val: 0x%X\n", ret, i, &Mem[i], Mem[i]);
        i++;
    } while (ret==4);

    while(1){
        statClockCycle++;
        printf("Cycle = %d \n", statClockCycle);
        printf("PC = %X\n", pc);
        statInst++;
        fetch();
        writeback();
        decode();
        execute();
        memoryaccess();
        // writeback();

        ifIdLatch[1] = ifIdLatch[0];
        idExLatch[1] = idExLatch[0];
        exMemLatch[1] = exMemLatch[0];
        memWbLatch[1] = memWbLatch[0];

        if (pc == 0xFFFFFFFF) break;
    }

    printf("\n");
    printf("Final R[2]= %d \n", regfile[2]);
    printf("Statistics of Execution = %d\n", statExec);
    printf("Total # of Instructions = %d\n", statInst);
    printf("Total # of Memory Operation Instruction = %d\n", statMem);
    printf("Total # of Register Operation Instruction = %d\n", statReg);
    printf("Total # of Branch Instruction = %d\n", statBranch);
    printf("Total # of Not Taken Branch = %d\n", statBranchNotT);
    printf("Total # of Jump Instruction = %d\n", statJump);
    printf("Total # of Clock Cycles = %d\n", statClockCycle);

    fclose(fd);
}
