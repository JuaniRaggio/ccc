#pragma once

#include "AbstractSyntaxTree.h"
#include <stdbool.h>

/* A multiset of identifier names borrowed from the AST (not owned). */
typedef struct {
	char **names;
	int count;
	int capacity;
} NameSet;

/* Collects the identifiers read and written by a statement subtree. When `program` is
 * given, a reference argument of a call is attributed as the callee reads/writes the
 * corresponding parameter; pass NULL to skip that (intra-body accesses only). */
void collectStmtAccesses(Stmt *stmt, Program *program, NameSet *reads, NameSet *writes);

/* Collects the names driven by a reference-output argument of a call (the callee writes
 * the corresponding reference parameter). In the generated hardware each such name is
 * driven by a module instance. */
void collectCallRefWrites(Stmt *stmt, Program *program, NameSet *driven);

void nameSetAdd(NameSet *set, char *name);
bool nameSetContains(const NameSet *set, const char *name);
bool nameSetsIntersect(const NameSet *a, const NameSet *b);
void nameSetFree(NameSet *set);
