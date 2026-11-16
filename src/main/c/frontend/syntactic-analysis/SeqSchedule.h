#pragma once

#include "AbstractSyntaxTree.h"
#include <stdbool.h>

/* How a __sequential body is cut into FSM cycles. The semantic analyzer (RAW check) and
 * the generator (state emission) both use this, so they always agree on which statements
 * share a clock.
 *
 * A region (a statement list) is a chain of segments. A maximal run of non-loop statements
 * is one segment and takes one state (one clock). Each loop is its own segment: a `for`
 * with an init takes an init state, then a condition-check state, its body region, and an
 * update state when it has an update; a `while` takes the check state and its body. */
typedef struct {
	bool isLoop;
	Stmt **stmts; /* When isLoop, stmts[0] is the loop. */
	int count;
} SeqSegment;

typedef struct {
	SeqSegment *segments;
	int count;
	Stmt **storage;
} SeqRegion;

bool seqIsLoop(const Stmt *stmt);

/* The body of a for or while loop. */
Stmt *seqLoopBody(Stmt *loop);

/* Segments a region given as a statement: a compound's list, or a single statement. */
SeqRegion seqRegionOf(Stmt *body);

/* Segments a region given as a statement list. */
SeqRegion seqRegionOfList(StmtList *list);

void seqRegionFree(SeqRegion *region);

/* Number of FSM states the region schedules, recursively (IDLE and DONE not included). */
int seqCountStates(Stmt *body);
