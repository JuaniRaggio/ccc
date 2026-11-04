#pragma once

#include "AbstractSyntaxTree.h"
#include <stdbool.h>

/* A multiset of identifier names borrowed from the AST (not owned). */
typedef struct {
	char **names;
	int count;
	int capacity;
} NameSet;

/* Collects the identifiers read and written by a statement subtree. */
void collectStmtAccesses(Stmt *stmt, NameSet *reads, NameSet *writes);

bool nameSetContains(const NameSet *set, const char *name);
bool nameSetsIntersect(const NameSet *a, const NameSet *b);
void nameSetFree(NameSet *set);
