// Scintilla source code edit control
/** @file Sci_Position.h
 ** Define the Sci_Position type used in Scintilla's external interfaces.
 **/
// Copyright 2015 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#ifndef SCI_POSITION_H
#define SCI_POSITION_H

#include <stdint.h>

// Position within a document, signed.
typedef intptr_t Sci_Position;

// Unsigned position, used for Lex/Fold startPos.
typedef uintptr_t Sci_PositionU;

// SCI_METHOD is the calling convention for ILexer/IDocument methods.
// On Windows x86 this is __stdcall; on x64 and other platforms it's default.
#ifdef _WIN32
#define SCI_METHOD __stdcall
#else
#define SCI_METHOD
#endif

#endif
