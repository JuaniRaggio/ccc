#include "SeqSchedule.h"
#include <stdlib.h>

bool
seqIsLoop(const Stmt *stmt) {
	return stmt != NULL && (stmt->kind == STMT_FOR || stmt->kind == STMT_WHILE);
}

Stmt *
seqLoopBody(Stmt *loop) {
	return loop->kind == STMT_FOR ? loop->forStmt.body : loop->whileStmt.body;
}

static SeqRegion
_segment(Stmt **items, int count) {
	SeqRegion region = { .segments = NULL, .count = 0, .storage = items };
	if (count == 0) {
		return region;
	}
	region.segments = malloc(count * sizeof(SeqSegment));
	int i = 0;
	while (i < count) {
		SeqSegment *segment = &region.segments[region.count++];
		segment->stmts = &items[i];
		if (seqIsLoop(items[i])) {
			segment->isLoop = true;
			segment->count = 1;
			++i;
		} else {
			segment->isLoop = false;
			segment->count = 0;
			while (i < count && !seqIsLoop(items[i])) {
				++segment->count;
				++i;
			}
		}
	}
	return region;
}

SeqRegion
seqRegionOfList(StmtList *list) {
	int count = 0;
	for (StmtList *node = list; node != NULL; node = node->next) {
		++count;
	}
	Stmt **items = count > 0 ? malloc(count * sizeof(Stmt *)) : NULL;
	int index = 0;
	for (StmtList *node = list; node != NULL; node = node->next) {
		items[index++] = node->stmt;
	}
	return _segment(items, count);
}

SeqRegion
seqRegionOf(Stmt *body) {
	if (body == NULL) {
		return _segment(NULL, 0);
	}
	if (body->kind == STMT_COMPOUND) {
		return seqRegionOfList(body->compound);
	}
	Stmt **items = malloc(sizeof(Stmt *));
	items[0] = body;
	return _segment(items, 1);
}

void
seqRegionFree(SeqRegion *region) {
	free(region->segments);
	free(region->storage);
	region->segments = NULL;
	region->storage = NULL;
	region->count = 0;
}

int
seqCountStates(Stmt *body) {
	SeqRegion region = seqRegionOf(body);
	int states = 0;
	for (int i = 0; i < region.count; ++i) {
		SeqSegment *segment = &region.segments[i];
		if (!segment->isLoop) {
			++states;
			continue;
		}
		Stmt *loop = segment->stmts[0];
		++states; /* condition check */
		if (loop->kind == STMT_FOR) {
			states += loop->forStmt.init != NULL ? 1 : 0;
			states += loop->forStmt.update != NULL ? 1 : 0;
		}
		states += seqCountStates(seqLoopBody(loop));
	}
	seqRegionFree(&region);
	return states;
}
