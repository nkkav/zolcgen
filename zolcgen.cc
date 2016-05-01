/*
 *     Copyright (c) 2006 Nikolaos Kavvadias
 *
 *     This software was written by Nikolaos Kavvadias, Ph.D. candidate
 *     at the Physics Department, Aristotle University of Thessaloniki,
 *     Greece.
 *
 *     This software is provided under the terms described in the "LICENSE"
 *     file.
 */
//
// zolcgen.cc
//
// This tool updates an assembly program with the proper ZOLC initialization
// sequence.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "salto.h"

#define DEBUG

#ifdef DEBUG
#  include <stdio.h>
#  define dbg_printf(args...) fprintf (stderr, args)
#  define _d_(arg) arg
#else
#  define dbg_printf(args...)
#  define _d_(arg)
#endif

#define  BB_FIRST   0
#define  BB_LAST    1


char  *outputFile = NULL;
char  *kernel_proc_name = NULL;

unsigned int OverheadMark[50];
int DptBbRange[50][2];
int PCEntryAddress[50],PCExitAddress[50];

int disable_ldsa=0;
int text_addr_val=0x1000;
unsigned int task_num=0;


// Function prototypes
void reverse(char s[]);
void itoa(int n, char s[]);


//
// Handle overhead instructions in the cfg.
// Overhead instructions (preceding the "overhead" mark instruction)
// will be either removed (overhead=2) or converted to NOP (overhead=1)
//
void HandleOverheadInstrs(CFG *cfg)
{
  INST   *nop, *inst;
  BB     *bb;
  char   instr_name[24];
  char   oper_str[24];
  unsigned int    nasm, nbb, i, j;
  int    overhead_val, overhead_cnt=1;

  nbb = cfg -> numberOfBB();

  for (i=0; i<nbb; i++)
  {
    bb = cfg -> getBB(i);

    nasm = bb -> numberOfAsm();

    for (j=0; j<(bb -> numberOfAsm()); j++)
    {
      inst = bb -> getAsm(j);

      // Get the instruction mnemonic
      sprintf(instr_name, "%s", inst -> getName());

      // if instruction is an "overhead"
      if (strcmp(instr_name, "overhead") == 0)
      {
        dbg_printf("%d: Read an overhead instruction (%d).\n", overhead_cnt, j);

        // Read the "state" operand of the overhead marker.
        // if state = 1, then replace the following instruction with a NOP
        // if state = 2, then remove the following instruction
        operand *op0 = inst -> getRawOperand(0);

        dbg_printf("overhead instr. has an imm. operand\n");

        op0->string(oper_str);
        dbg_printf("value = %s\n", oper_str);

        overhead_val = atoi(oper_str);

        if (overhead_val == 2)
        {
	  // remove the "overhead" marker
          bb -> extractAsm(j);
	  // remove the succeeding instruction
          bb -> extractAsm(j);
        }

        if (overhead_val == 1)
        {
          bb -> extractAsm(j);
          bb -> extractAsm(j);
	  nop = newAsm("nop");
	  bb -> insertAsm(j, nop);
        }

        overhead_cnt++;
      }
    }
  }
}

//
// Handle all the instructions that the "loop" macro-instruction expands to.
// These comprise a sequence of "ldsa", "ldsi", "ldss", and "ldsf" instructions.
//
void RemoveOverheadInstrs(CFG *cfg)
{
  HandleOverheadInstrs(cfg);
  HandleOverheadInstrs(cfg);
  HandleOverheadInstrs(cfg);
}

void HandleLdsLoopInstrs(CFG *cfg, char lds_name[24])
{
  INST   *inst;
  BB     *bb, *bb_first;
  char   instr_name[24];
  unsigned int    nasm, nbb, i, j;
  int    overhead_cnt=1;
  int    bb_first_instr_cnt=1;

  nbb = cfg -> numberOfBB();

  // Read the first basic block of the procedure
  bb_first = cfg -> getBB(0);

  for (i=0; i<nbb; i++)
  {
    bb = cfg -> getBB(i);

    nasm = bb -> numberOfAsm();

    for (j=0; j<(bb -> numberOfAsm()); j++)
    {
      inst = bb -> getAsm(j);

      // Get the instruction mnemonic
      sprintf(instr_name, "%s", inst -> getName());

      // if instruction has been derived (by expansion) from a "loop"
      // macro-instruction.
      // actually just read the first instruction of the group,
      // should be an "lds[a|i|s|f]"
      if (strcmp(instr_name, lds_name) == 0)
      {
        dbg_printf("%d: Read an %s instruction (%d).\n", overhead_cnt, lds_name, j);

        // First remove (extract) this instruction from the current basic block
        // (bb)
        // special case if this the last basic block (following the ret from procedure)
        // then adjust the offset properly (-2 instead of -3)
	if (i == nbb-1)
          bb -> extractAsm((bb -> numberOfAsm())-2);
        else
          bb -> extractAsm((bb -> numberOfAsm())-5);

        // Then, insert "lds[a|i|s|f]" in the first basic block (bb_first) at a proper
        // position
	// if LDSA instructions should only be extracted (and not moved)
	if (disable_ldsa==1 && strcmp(lds_name, "ldsa")==0)
	{
	}
	else
	{
          bb_first -> insertAsm(bb_first_instr_cnt, inst);
          bb_first_instr_cnt++;
	}

	overhead_cnt++;
      }
    }
  }
}

//
// Handle all the instructions that the "loop" macro-instruction expands to.
// These comprise a sequence of "ldsa", "ldsi", "ldss", and "ldsf" instructions.
//
void MoveLoopInstrs(CFG *cfg)
{
  HandleLdsLoopInstrs(cfg, "ldsf");
  HandleLdsLoopInstrs(cfg, "ldss");
  HandleLdsLoopInstrs(cfg, "ldsi");
  HandleLdsLoopInstrs(cfg, "ldsa");
}

void HandleLdsTaskInstrs(CFG *cfg, char lds_name[24])
{
  INST   *inst;
  BB     *bb, *bb_first;
  char   instr_name[24];
  unsigned int    nasm, nbb, i, j;
  int    overhead_cnt=1;
  int    bb_first_instr_cnt=1;

  nbb = cfg -> numberOfBB();

  // Read the first basic block of the procedure
  bb_first = cfg -> getBB(0);

  for (i=0; i<nbb; i++)
  {
    bb = cfg -> getBB(i);

    nasm = bb -> numberOfAsm();

    for (j=0; j<(bb -> numberOfAsm()); j++)
    {
      inst = bb -> getAsm(j);

      // Get the instruction mnemonic
      sprintf(instr_name, "%s", inst -> getName());

      if (strcmp(instr_name, lds_name) == 0)
      {
        dbg_printf("%d: Read an %s instruction (%d).\n", overhead_cnt, lds_name, j);

        bb -> extractAsm(j);
        bb_first -> insertAsm(bb_first_instr_cnt, inst);
        bb_first_instr_cnt++;
        overhead_cnt++;

        inst = bb -> getAsm(j);
        bb -> extractAsm(j);
        bb_first -> insertAsm(bb_first_instr_cnt, inst);
        bb_first_instr_cnt++;
        overhead_cnt++;
      }
    }
  }
}

//
// Handle all the instructions that the "task" macro-instruction expands to.
// These expand to corresponding "ldst" instructions.
//
void MoveTaskInstrs(CFG *cfg)
{
  HandleLdsTaskInstrs(cfg, "ldst");
}

//
// Handle dpt information-carrying instructions in the cfg.
// These are not real instructions and after aquiring the relevant information
// (first and last basic block ids of a DPTI), they are removed.
//
void HandleDptInstrs(CFG *cfg)
{
  INST   *inst;
  BB     *bb;
  char   instr_name[24];
  char   oper_str[24];
  unsigned int    nasm, nbb, i, j;
  int    id_val=0, bb_first_val=0, bb_last_val=0, overhead_cnt=1;

  nbb = cfg -> numberOfBB();

  for (i=0; i<nbb; i++)
  {
    bb = cfg -> getBB(i);

    nasm = bb -> numberOfAsm();

    for (j=0; j<(bb -> numberOfAsm()); j++)
    {
      inst = bb -> getAsm(j);

      // Get the instruction mnemonic
      sprintf(instr_name, "%s", inst -> getName());

      // if instruction is a "dpti"
      if (strcmp(instr_name, "dpti") == 0)
      {
        dbg_printf("%d: Read a dpti instruction (%d).\n", overhead_cnt, j);

        // Read the operands of the dpti marker.
        operand *op0 = inst -> getRawOperand(0);
        operand *op1 = inst -> getRawOperand(1);
        operand *op2 = inst -> getRawOperand(2);

        op0->string(oper_str);
        id_val = atoi(oper_str);
        op1->string(oper_str);
        bb_first_val = atoi(oper_str);
        op2->string(oper_str);
        bb_last_val = atoi(oper_str);
        dbg_printf("%d: id=%d\tbb_first=%d\tbb_last=%d\n", overhead_cnt, id_val, bb_first_val, bb_last_val);

        // Set the entry
        DptBbRange[task_num][0] = bb_first_val;
        DptBbRange[task_num][1] = bb_last_val;

        // Remove the "dpti" instruction
        bb -> extractAsm(j);

        overhead_cnt++;
        task_num++;
      }
    }
  }

  // NASTY HACK: Correcting the final DptBbRange[][1] entry.
  // Corresponds to the exit of task bwd0(0).
  DptBbRange[task_num-1][1]--;

  if (strcmp(cfg->getName(), kernel_proc_name) == 0)
  {
    for (i=0; i<task_num; i++)
    {
      dbg_printf("DptBbRange[%d][0] = %d\n", i, DptBbRange[i][0]);
      dbg_printf("DptBbRange[%d][1] = %d\n", i, DptBbRange[i][1]);
    }
  }
}

int GetPCAddressOffset(char inp_proc_name[48], int base_text_addr)
{
  CFG    *cfg;
  BB     *bb;
  INST   *inst;
  char   proc_name[24];
  int    ncfg, nbb, nasm, i, j, k;
  int    pc_address;

  pc_address = base_text_addr;

  ncfg = numberOfCFG();

  for (i=0; i<ncfg; i++)
  {
    cfg = getCFG(i);

    nbb = cfg -> numberOfBB();

    // Get the procedure name
    sprintf(proc_name, "%s", cfg -> getName());

    for (j=0; j<nbb; j++)
    {
      bb = cfg -> getBB(j);

      nasm = bb -> numberOfAsm();

      for (k=0; k<nasm; k++)
      {
        inst = bb -> getAsm(k);

        if (strcmp(proc_name, inp_proc_name) == 0)
        {
          if (j == 0)
          {
            return (pc_address);
          }
        }

        pc_address += 4;
      }
    }
  }

  return (pc_address);
}

int GetBBPCAddressOffset(char inp_proc_name[48], int base_text_addr, int bb_num, int mode)
{
  CFG    *cfg;
  BB     *bb;
  INST   *inst;
  char   proc_name[24];
  int    ncfg, nbb, nasm, ninst, i, j, k, m;
  int    pc_address;

  pc_address = base_text_addr;

  ncfg = numberOfCFG();

  for (i=0; i<ncfg; i++)
  {
    cfg = getCFG(i);

    nbb = cfg -> numberOfBB();

    // Get the procedure name
    sprintf(proc_name, "%s", cfg -> getName());

    for (j=0; j<nbb; j++)
    {
      bb = cfg -> getBB(j);

      ninst = bb -> numberOfInstructions();
      nasm = bb -> numberOfAsm();

      // Counts "real" assembly instructions
      m = 0;

      for (k=0; k<ninst; k++)
      {
        inst = bb -> getInstruction(k);

        if (strcmp(proc_name, inp_proc_name) == 0)
        {
	  if ((m==0 || nasm==0) && mode==BB_FIRST && j==bb_num)
	  {
	    return (pc_address);
	  }
	  else if ((m==nasm-1 || nasm==0) && mode==BB_LAST && j==bb_num && bb_num!=nbb-1)
	  {
	    return (pc_address);
	  }
        }

        if (inst -> isAsm())
          m++;

	if (inst -> isAsm())
          pc_address += 4;
      }
    }
  }

  return (pc_address);
}

void CreatePCEntryExitInfo(CFG *cfg, char inp_proc_name[48], int base_text_addr)
{
  char   proc_name[24];
  int    nbb;
  unsigned int i;
  int    pc_address = 0;

  pc_address = GetPCAddressOffset(inp_proc_name, base_text_addr);

  // Get the procedure name
  sprintf(proc_name, "%s", cfg -> getName());

  nbb = cfg -> numberOfBB();
  dbg_printf( "Number of BBs for this procedure: %d\n", nbb);

//  if ( strcmp(proc_name, kernel_proc_name) == 0)
  if (strcmp(proc_name, inp_proc_name) == 0)
  {
    for (i=0; i<50; i++)
    {
      PCEntryAddress[i] = 0;
      PCExitAddress[i]  = 0;
    }
  }

  if (strcmp(proc_name, inp_proc_name) == 0)
  {
    for (i=0; i<task_num; i++)
    {
      // Get the address of the FIRST instruction in the task
      // if this bb has a "task entry" marker
      PCEntryAddress[i] = GetBBPCAddressOffset(inp_proc_name, base_text_addr, DptBbRange[i][0], BB_FIRST);

      // Get the address of the LAST instruction in the task
      // if this bb has a "task exit" marker
      PCExitAddress[i]  = GetBBPCAddressOffset(inp_proc_name, base_text_addr, DptBbRange[i][1], BB_LAST);

      // NASTY HACK (unfortunate): Fix an issue with the bwd0 task exit
      if (i==task_num-1)
      {
        PCExitAddress[i] += 4;
      }
    }
  }

  if (strcmp(proc_name, inp_proc_name) == 0)
  {
    for (i=0; i<task_num; i++)
    {
      dbg_printf("ldsn\t%d, 0x%x\n",i, PCEntryAddress[i]);
      dbg_printf("ldsx\t%d, 0x%x\n",i, PCExitAddress[i]);
    }
  }

  if (strcmp(proc_name, inp_proc_name) == 0)
  {
    for (i=0; i<task_num; i++)
    {
      dbg_printf("Task %d entry at 0x%x\n", i, GetBBPCAddressOffset(inp_proc_name, base_text_addr, DptBbRange[i][0], BB_FIRST));
      dbg_printf("Task %d exit at 0x%x\n", i, GetBBPCAddressOffset(inp_proc_name, base_text_addr, DptBbRange[i][1], BB_LAST));
    }
  }
}

//
// Handle dpt information-carrying instructions in the cfg.
// These are not real instructions and after aquiring the relevant information
// (first and last basic block ids of a DPTI), they are removed.
//
void CreatePCEntryExitInstrs(CFG *cfg, char inp_proc_name[48])
{
  INST   *inst_ldsn, *inst_ldsx;
  BB     *bb_first;
  char   proc_name[24];
  int    nbb, i;
  int    bb_first_instr_cnt=1, overhead_cnt=1;

  // Get the procedure name
  sprintf(proc_name, "%s", cfg -> getName());

  nbb = cfg -> numberOfBB();

  // Read the first basic block of the procedure
  bb_first = cfg -> getBB(0);

  if (strcmp(proc_name, inp_proc_name) == 0)
  {
    for (i=0; i<(int)task_num; i++)
    {
      // Create the destination (0) and source operands (1) of an ldsn instruction
      OperandInfo op0_ldsn = OperandInfo(i);
      OperandInfo op1_ldsn = OperandInfo(PCEntryAddress[i]);

      // Create the destination (0) and source operands (1) of an ldsx instruction
      OperandInfo op0_ldsx = OperandInfo(i);
      OperandInfo op1_ldsx = OperandInfo(PCExitAddress[i]);

      // Create a new assembly instruction (ldsn) for setting a task entry PC
      inst_ldsn = newAsm("ldsn", "o,o", 2, &op0_ldsn, &op1_ldsn);

      // Insert the instruction at position "bb_first_instr_cnt" of BB "bb_first"
      bb_first -> insertAsm(bb_first_instr_cnt, inst_ldsn);
      bb_first_instr_cnt++;      

      // Create a new assembly instruction (ldsx) for setting a task exit PC
      inst_ldsx = newAsm("ldsx", "o,o", 2, &op0_ldsx, &op1_ldsx);

      // Insert the instruction at position "bb_first_instr_cnt" of BB "bb_first"
      bb_first -> insertAsm(bb_first_instr_cnt, inst_ldsx);
      bb_first_instr_cnt++;

      overhead_cnt++;
    }
  }
}

void addEndOfProgCode(CFG *cfg)
{
}

void Salto_hook()
{
  CFG   *cfg;
//  BB    *bb;
  int    i, /*j,*/ ncfg/*, nbb, nasm*/;
  FILE  *f;

  if (strcmp(getTargetName(), "SUIFVM"))
  {
    fprintf(stderr, "Sorry, this tool supports only SUIFvm (suifrm-zolc) assembly code\n");
    exit(1);
  }

//  nbbs_in_program = numberOfBBInProgram();

  for (i=0; i<50; i++)
  {
    DptBbRange[i][0] = 0;
    DptBbRange[i][1] = 0;
  }

  ncfg = numberOfCFG();

  for (i=0; i<ncfg; i++)
  {
    cfg = getCFG(i);

    // move loop (ldsa, ldsi, ldss, ldsf) instructions to the first BB in the procedure
    MoveLoopInstrs(cfg);
  }

  for (i=0; i<ncfg; i++)
  {
    cfg = getCFG(i);

    // move task (ldst) instructions to the first BB in the procedure
    MoveTaskInstrs(cfg);
  }

  for (i=0; i<ncfg; i++)
  {
    cfg = getCFG(i);

    // remove overhead instructions in the input assembly program
    RemoveOverheadInstrs(cfg);
  }

  for (i=0; i<ncfg; i++)
  {
    cfg = getCFG(i);

    // pass the "dpti" information to array DptBbRange[][2]
    HandleDptInstrs(cfg);
  }

  for (i=0; i<ncfg; i++)
  {
    cfg = getCFG(i);

    // create task entry-exit PC address info
    CreatePCEntryExitInfo(cfg, kernel_proc_name, text_addr_val);
  }

  for (i=0; i<ncfg; i++)
  {
    cfg = getCFG(i);

    // create the actual "ldsn" and "ldsx" instructions
    CreatePCEntryExitInstrs(cfg, kernel_proc_name);
  }

  // generates instrumented code
  if (outputFile)
  {
    f = fopen(outputFile, "w");

    if (!f)
    {
      perror("fopen");
      f = stdout;
    }
  }
  else
  {
    f = stdout;
  }

  produceCode(f);

  fclose(f);
}


void
Salto_init_hook(int c, char *v[])
{
  int    i;

  for (i=1; i<c-1; i++)
  {
    // -o flag is used to specify output file
    if (strcmp(v[i], "-o")==0)
    {
      outputFile = v[i+1];
    }
    else if (strcmp(v[i], "-disable-ldsa")==0)
    {
      disable_ldsa = 1;
    }
    else if (strcmp(v[i], "-text-addr")==0)
    {
      text_addr_val = atoi(v[i+1]);
    }
    // -proc flag is used for specifying the name of the procedure for ZOLC
    // code generation
    else if (strcmp(v[i], "-proc")==0)
    {
      kernel_proc_name = v[i+1];
    }
  }

  dbg_printf("disable_ldsa = %d\n",disable_ldsa);
  dbg_printf("text_addr = %d\n",text_addr_val);
  dbg_printf("kernel_proc_name = %s\n",kernel_proc_name);
}


void Salto_end_hook()
{
}

int updateDelay(int delay, INST * /*src*/, INST * /*dst*/, dependence /*t*/)
{
  return delay;
}

/* reverse: reverse string s in place */
void reverse(char s[])
{
  int c, i, j;

  for (i = 0, j = strlen(s)-1; i < j; i++, j--)
  {
    c = s[i];

    s[i] = s[j];
    s[j] = c;
  }
}


/* itoa: convert n to characters in s */
void itoa(int n, char s[])
{
  int i, sign;

  if ((sign = n) < 0) /* record sign */
    n = -n; /* make n positive */

  i = 0;

  do 
  { /* generate digits in reverse order */
    s[i++] = n % 10 + '0'; /* get next digit */

  } while ((n /= 10) > 0); /* delete it */

  if (sign < 0)
    s[i++] = '-';

  s[i] = '\0';

  reverse(s);
}
