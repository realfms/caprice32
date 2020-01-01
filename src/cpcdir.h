
/*
------------------------------------------------------------------------------

    =====
    CPCFS  --  c p c f s . h	Variable, Structures, Prototypes
    =====

	Version 0.85                    (c) Derik van Zuetphen
------------------------------------------------------------------------------
*/

#ifndef CPCFS_H_INCLUDED
#define CPCFS_H_INCLUDED

/***********
  Directory
 ***********/

/*******
  Image
 *******/

void close_image();
int  open_image(char *name);
void cpcfs_init();
void cpcfs_finish();
int cpcfs_getNfiles();
char *cpcfs_fileName(int nFile);

#endif
