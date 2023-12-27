/*
 * Copyright (C) 2006-2022 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
 The test shows how wrappers may be implemented in DLL loaded in runtime.
 The dopen() is being called from application space. But it can't be called
 before libc is initialized.
 In this example I call dlopen before main().
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <string>

#include "pin.H"
#include "tool_macros.h"
using std::cerr;
using std::endl;

#define MALLOC_LIB "libc.so"

#define SHM_ID_ENV "CARVING_SHM_ID"
#define NUM_SHM_ENTRY 1024

typedef struct shm_entry_ {
  char* ptr;
  int size;
  char is_malloc;
} shm_entry;

int shm_id = 0;
// shm_entry *shm_map,
// first 16 bytes (sizeof shm_entry) is used to store
// 1. number of entries (first 4 bytes)
// 2. charater writing lock (5th byte)
char* shm_map = nullptr;

/* ===================================================================== */

INT32 Usage() {
  cerr << "This pin tool inserts a user-written version of malloc() and free() "
          "into the application.\n"
          "\n";
  cerr << KNOB_BASE::StringKnobSummary();
  cerr << endl;
  return -1;
}

/* ===================================================================== */
/* Definitions for Probe mode */
/* ===================================================================== */

typedef typeof(malloc)* MallocType;
typedef typeof(free)* FreeType;
typedef typeof(exit)* ExitType;

bool enable_record = false;

MallocType origMalloc = 0;
FreeType origFree = 0;
ExitType origExit = 0;

/* ===================================================================== */
/* Probe mode tool */
/* ===================================================================== */

VOID* MallocWrapperInTool(size_t size) {
  if (!enable_record) {
    return (*origMalloc)(size);
  }

  void* res = (*origMalloc)(size);

  char shm_lock = ((char*)shm_map)[4];

  if (shm_lock) {
    return res;
  }

  int cur_num_entry = ((int*)shm_map)[0];

  if (cur_num_entry >= (NUM_SHM_ENTRY - 1)) {
    cerr << "Warn: Too many malloc/free calls" << endl;
    return res;
  }

  cerr << "Adding malloc entry : " << res << "," << size << endl;

  ((int*)shm_map)[0] = cur_num_entry + 1;

  shm_entry* entry = &((shm_entry*)shm_map)[cur_num_entry + 1];
  entry->ptr = (char*)res;
  entry->size = size;
  entry->is_malloc = 1;

  return res;
}

VOID FreeWrapperInTool(void* p) {
  if (!enable_record) {
    ASSERTX(origFree != 0);
    (*origFree)(p);
    return;
  }

  (*origFree)(p);

  char shm_lock = ((char*)shm_map)[4];

  if (shm_lock) {
    return;
  }

  int cur_num_entry = ((int*)shm_map)[0];
  if (cur_num_entry >= (NUM_SHM_ENTRY - 1)) {
    cerr << "Warn: Too many malloc/free calls" << endl;
    return;
  }

  cerr << "Adding free entry : " << p << endl;

  ((int*)shm_map)[0] = cur_num_entry + 1;

  shm_entry* entry = &((shm_entry*)shm_map)[cur_num_entry + 1];
  entry->ptr = (char*)p;
  entry->size = 0;
  entry->is_malloc = 0;
  return;
}

VOID EXITWarpperInTool(int code) {
  enable_record = false;
  cerr << "Exit wrapper called\n";

  shmctl(shm_id, IPC_RMID, 0);
  unsetenv(SHM_ID_ENV);

  origExit(code);
}

VOID MainRtnCallback() {
  shm_id =
      shmget(IPC_PRIVATE, sizeof(shm_entry) * NUM_SHM_ENTRY, IPC_CREAT | 0666);
  shm_map = (char*)shmat(shm_id, 0, 0);

  if (shm_id == -1 || shm_map == nullptr) {
    cerr << "shmget or shmat failed" << endl;
    shmctl(shm_id, IPC_RMID, 0);
    exit(1);
  }

  int _len = snprintf(NULL, 0, "%d", shm_id);
  if (_len < 0) {
    cerr << "snprintf failed" << endl;
    shmctl(shm_id, IPC_RMID, 0);
    exit(1);
  }

  pid_t pid = getpid();
  std::string shm_id_fn = "/tmp/pin_shm_id_" + std::to_string(pid);
  std::ofstream shm_id_file(shm_id_fn);

  if (!shm_id_file.is_open()) {
    cerr << "Can't open shm_id file" << endl;
    shmctl(shm_id, IPC_RMID, 0);
    exit(1);
  }

  shm_id_file << shm_id << endl;

  shm_id_file.close();

  ((int*)shm_map)[0] = 0;
  ((char*)shm_map)[4] = 0;

  enable_record = true;
}

VOID ImageLoad(IMG img, VOID* v) {
  if (IMG_IsMainExecutable(img)) {
    RTN mainRtn = RTN_FindByName(img, "_main");
    if (!RTN_Valid(mainRtn)) mainRtn = RTN_FindByName(img, "main");

    if (!RTN_Valid(mainRtn)) {
      cerr << "Can't find the main routine in " << IMG_Name(img) << endl;
      exit(1);
    }
    ASSERTX(RTN_InsertCallProbed(mainRtn, IPOINT_BEFORE,
                                 AFUNPTR(MainRtnCallback), IARG_END));
    return;
  }

  if (!strstr(IMG_Name(img).c_str(), MALLOC_LIB)) {
    return;
  }

  // Replace malloc and free in application libc with wrappers
  RTN mallocRtn = RTN_FindByName(img, C_MANGLE("malloc"));
  ASSERTX(RTN_Valid(mallocRtn));

  if (!RTN_IsSafeForProbedReplacement(mallocRtn)) {
    cerr << "Cannot replace malloc in " << IMG_Name(img) << endl;
    exit(1);
  }

  RTN freeRtn = RTN_FindByName(img, C_MANGLE("free"));
  ASSERTX(RTN_Valid(freeRtn));

  if (!RTN_IsSafeForProbedReplacement(freeRtn)) {
    cerr << "Cannot replace free in " << IMG_Name(img) << endl;
    exit(1);
  }

  origMalloc =
      (MallocType)RTN_ReplaceProbed(mallocRtn, AFUNPTR(MallocWrapperInTool));

  origFree = (FreeType)RTN_ReplaceProbed(freeRtn, AFUNPTR(FreeWrapperInTool));

  RTN exitRtn = RTN_FindByName(img, C_MANGLE("exit"));
  ASSERTX(RTN_Valid(exitRtn));

  if (!RTN_IsSafeForProbedReplacement(exitRtn)) {
    cerr << "Cannot replace exit in " << IMG_Name(img) << endl;
    exit(1);
  }

  origExit = (ExitType)RTN_ReplaceProbed(exitRtn, AFUNPTR(EXITWarpperInTool));

  return;
}

/* ===================================================================== */
/* main */
/* ===================================================================== */

int main(int argc, CHAR* argv[]) {
  PIN_InitSymbols();

  if (PIN_Init(argc, argv)) {
    return Usage();
  }

  IMG_AddInstrumentFunction(ImageLoad, 0);

  PIN_StartProgramProbed();

  return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
