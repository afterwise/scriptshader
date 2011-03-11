
#pragma once

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SS_MAX_NAME_SIZE (0x40)
#define SS_MAX_NUM_VARS (0x40)
#define SS_MAX_NUM_FUNCS (0x80)
#define SS_MAX_NUM_STACK (0x40)

enum
{
	SS_EOF_TYPE,
	SS_NAME_TYPE,
	SS_NUMBER_TYPE,
	SS_PUNCT_TYPE
};

enum
{
	SS_STOP_OP,
	SS_PUSH_OP,
	SS_CONST_OP,
	SS_LOAD_OP,
	SS_STORE_OP,
	SS_SEL_OP,
	SS_MIN_OP,
	SS_MAX_OP,
	SS_CLAMP_OP,
	SS_SATURATE_OP,
	SS_ADD_OP,
	SS_SUB_OP,
	SS_MUL_OP,
	SS_DIV_OP,
	SS_FLOOR_OP,
	SS_CEIL_OP,
	SS_ABS_OP,
	SS_SQR_OP,
	SS_SQRT_OP,
	SS_POW_OP,
	SS_EXP_OP,
	SS_SIN_OP,
	SS_COS_OP,
	SS_ASIN_OP,
	SS_ACOS_OP
};

struct SsFunc
{
	char name[SS_MAX_NAME_SIZE];
	char* code;
};

struct SsFuncInfo
{
	struct SsFunc* func;
	int argc;
	char argv[SS_MAX_NUM_VARS][SS_MAX_NAME_SIZE];
};

struct SsRuntime
{
	int fcnt;
	struct SsFunc func[SS_MAX_NUM_FUNCS];
};

struct SsParseBuffer
{
	struct SsRuntime* rt;
	char* rp;
	char* wp;
	int line;
	int nest;
	int type;
	union {
		int punct;
		char name[SS_MAX_NAME_SIZE];
		float num;
	};
	struct SsFuncInfo* cur;
	struct SsFuncInfo info[SS_MAX_NUM_FUNCS];
};

#define ssError(...) do { fprintf(stderr, "ssError: " __VA_ARGS__); return -1; } while (0)
#define ssCheck(x) do { if ((x) < 0) return -1; } while (0)

#define ssAddArg(pb,a) \
	do { \
		if ((pb)->cur->argc == SS_MAX_NUM_VARS) \
			ssError("%d: Too many variables\n", (pb)->line); \
		strcpy((pb)->cur->argv[(pb)->cur->argc++], (a)); \
	} while (0)

#define ssEmitCode(pb,c) \
	do { \
		*(pb)->wp++ = (unsigned char) (c); \
		assert((pb)->wp <= (pb)->rp); \
	} while (0)

#define ssEmitConst(pb,v) \
	do { \
		const float u = (v); \
		memcpy((pb)->wp, &(u), sizeof(u)); \
		(pb)->wp += sizeof(u); \
		assert((pb)->wp <= (pb)->rp); \
	} while (0)

static int ssNextToken(struct SsParseBuffer* pb)
{
	char* j = pb->rp;
	assert(j >= pb->wp);

	while (isspace(*j)) {
		pb->line += !(*j++ - '\n');

		if (!(j[0] - '/') && !(j[1] - '/'))
			while (*j && *j - '\n') j++;
	}

	char* k = j;

	if (*k == 0)
		pb->type = SS_EOF_TYPE;
	else if (isalpha(*k)) {
		pb->type = SS_NAME_TYPE;
		do k++; while (isalnum(*k));

		if (k - j > SS_MAX_NAME_SIZE - 1)
			ssError("%d: Name is too long\n", pb->line);

		memcpy(pb->name, j, k - j);
		pb->name[k - j] = 0;
	}
	else if (isdigit(*k) || (k[0] == '-' && isdigit(k[1]))) {
		pb->type = SS_NUMBER_TYPE;
		pb->num = (float) strtod(k, &k);
	}
	else {
		pb->type = SS_PUNCT_TYPE;
		pb->punct = *k++;
	}

	pb->rp = k;
	return 0;
}

static int ssResolve(struct SsParseBuffer* pb, struct SsFuncInfo* fi, const char* name)
{
	for (int i = 0, n = fi->argc; i < n; ++i)
		if (!strcmp(fi->argv[i], name))
			return i;

	ssError("%d: Failed to resolve variable `%s'\n", pb->line, name);
}

static int ssBuiltin(const char* name)
{
	static struct { const char* name; int op; } builtin[] = {
		{"sel", SS_SEL_OP | 0x300},
		{"min", SS_MIN_OP | 0x200},
		{"max", SS_MAX_OP | 0x200},
		{"clamp", SS_CLAMP_OP | 0x300},
		{"saturate", SS_SATURATE_OP | 0x100},
		{"floor", SS_FLOOR_OP | 0x100},
		{"ceil", SS_CEIL_OP | 0x100},
		{"abs", SS_ABS_OP | 0x100},
		{"sqr", SS_SQR_OP | 0x100},
		{"sqrt", SS_SQRT_OP | 0x100},
		{"pow", SS_POW_OP | 0x200},
		{"exp", SS_EXP_OP | 0x100},
		{"sin", SS_SIN_OP | 0x100},
		{"cos", SS_COS_OP | 0x100},
		{"asin", SS_ASIN_OP | 0x100},
		{"acos", SS_ACOS_OP | 0x100}
	};

	for (unsigned i = 0; i < sizeof(builtin) / sizeof(builtin[0]); ++i)
		if (!strcmp(builtin[i].name, name))
			return builtin[i].op;

	return -1;
}

static int ssParseExpr(struct SsParseBuffer* pb, int isargs);

static int ssParseTerm(struct SsParseBuffer* pb, int isargs)
{
	int i, op;

	ssCheck(ssNextToken(pb));

	if (pb->type == SS_PUNCT_TYPE) {
		if (pb->punct == '(') {
			++pb->nest;
			ssCheck(ssParseExpr(pb, 0));
		}
		else if (pb->punct == '-') {
			ssEmitCode(pb, SS_CONST_OP);
			ssEmitConst(pb, 0.f);
			ssEmitCode(pb, SS_PUSH_OP);
			ssCheck(ssParseExpr(pb, isargs));
			ssEmitCode(pb, SS_SUB_OP);
		}
		else ssError("%d: Unexpected token in expression `%c'\n", pb->line, pb->punct);
	}
	else if (pb->type == SS_NAME_TYPE) {
		if ((op = ssBuiltin(pb->name)) >= 0) {
			ssCheck(ssNextToken(pb));

			if (pb->type != SS_PUNCT_TYPE || pb->punct != '(')
				ssError("%d: Function used as variable\n", pb->line);

			for (i = 0; i < (op >> 8) - 1; ++i) {
				ssCheck(ssParseExpr(pb, 1));
				ssEmitCode(pb, SS_PUSH_OP);
			}

			ssCheck(ssParseExpr(pb, 1));
			ssEmitCode(pb, (op & 0xff));
		}
		else if (!strcmp(pb->name, "pi")) {
			ssEmitCode(pb, SS_CONST_OP);
			ssEmitConst(pb, M_PI);
		}
		else {
			ssCheck(i = ssResolve(pb, pb->cur, pb->name));
			ssEmitCode(pb, SS_LOAD_OP);
			ssEmitCode(pb, i);
		}
	}
	else if (pb->type == SS_NUMBER_TYPE) {
		ssEmitCode(pb, SS_CONST_OP);
		ssEmitConst(pb, pb->num);
	}
	else ssError("%d: Unexpected end-of-file in expression\n", pb->line);

	return 0;
}

static int ssParseExpr(struct SsParseBuffer* pb, int isargs)
{
	int op;

	ssCheck(ssParseTerm(pb, isargs));

	while (pb->type != SS_PUNCT_TYPE || (pb->punct != ',' && pb->punct != ';')) {
		ssCheck(ssNextToken(pb));

		if (pb->type != SS_PUNCT_TYPE)
			ssError("%d: Expected function call, operator or end of expression\n", pb->line);

		if (pb->punct == ',') {
			if (!isargs)
				ssError("%d: Unexpected comma not in function call\n", pb->line);
			break;
		}

		if (pb->punct == ';') {
			if (pb->nest > 0)
				ssError("%d: Unexpected semi-colon inside parenthesis\n", pb->line);
			break;
		}

		if (pb->punct == ')') {
			if (!isargs && --pb->nest < 0)
				ssError("%d: Unbalanced parenthesises in expression\n", pb->line);
			break;
		}

		if (pb->punct == '+') op = SS_ADD_OP;
		else if (pb->punct == '-') op = SS_SUB_OP;
		else if (pb->punct == '*') op = SS_MUL_OP;
		else if (pb->punct == '/') op = SS_DIV_OP;
		else ssError("%d: Unknown operator in expression `%c'\n", pb->line, pb->punct);

		ssEmitCode(pb, SS_PUSH_OP);
		ssCheck(ssParseTerm(pb, isargs));
		ssEmitCode(pb, op);
	}

	return 0;
}

static int ssParseAssign(struct SsParseBuffer* pb)
{
	int i;

	ssCheck(i = ssResolve(pb, pb->cur, pb->name));
	ssCheck(ssNextToken(pb));

	if (pb->type != SS_PUNCT_TYPE || pb->punct != '=')
		ssError("%d: Expected assignment operator in statement\n", pb->line);

	ssCheck(ssParseExpr(pb, 0));
	ssEmitCode(pb, SS_STORE_OP);
	ssEmitCode(pb, i);
	return 0;
}

static int ssParseVar(struct SsParseBuffer* pb)
{
	ssCheck(ssNextToken(pb));

	if (pb->type != SS_NAME_TYPE)
		ssError("%d: Expected variable name in declaration\n", pb->line);

	ssAddArg(pb, pb->name);
	return ssParseAssign(pb);
}

static int ssParseStmt(struct SsParseBuffer* pb)
{
	if (pb->type != SS_NAME_TYPE)
		ssError("%d: Expected name at beginning of statement\n", pb->line);

	if (!strcmp(pb->name, "float"))
		return ssParseVar(pb);

	return ssParseAssign(pb);
}

static int ssParseFunc(struct SsParseBuffer* pb)
{
	ssCheck(ssNextToken(pb));

	if (pb->type != SS_NAME_TYPE)
		ssError("%d: Expected name in function declaration\n", pb->line);

	strcpy(pb->cur->func->name, pb->name);
	pb->cur->func->code = pb->wp;

	ssCheck(ssNextToken(pb));

	if (pb->type != SS_PUNCT_TYPE || pb->punct != '(')
		ssError("%d: Expected open-parenthesis in function declaration\n", pb->line);

	for (int x = 0;; ++x) {
		ssCheck(ssNextToken(pb));

		if (pb->type == SS_PUNCT_TYPE) {
			if (pb->punct == ')')
				break;

			if (pb->punct == ',' && x > 0)
				ssCheck(ssNextToken(pb));
		}

		if (pb->type != SS_NAME_TYPE)
			ssError("%d: Expected argument name in function declaration\n", pb->line);

		ssAddArg(pb, pb->name);
	}

	ssCheck(ssNextToken(pb));

	if (pb->type != SS_PUNCT_TYPE || pb->punct != '{')
		ssError("%d: Expected open-bracket in function declaration\n", pb->line);

	for (;;) {
		ssCheck(ssNextToken(pb));

		if (pb->type == SS_PUNCT_TYPE && pb->punct == '}') {
			ssEmitCode(pb, SS_STOP_OP);
			return 0;
		}

		ssCheck(ssParseStmt(pb));
	}
}

static int ssParse(struct SsRuntime* rt, struct SsParseBuffer* pb, char* p, size_t n)
{
	pb->rt = rt;
	pb->rp = p;
	pb->wp = p;
	pb->line = 1;
	pb->nest = 0;
	pb->type = SS_EOF_TYPE;

	for (rt->fcnt = 0;; ++rt->fcnt) {
		pb->cur = &pb->info[rt->fcnt];
		pb->cur->func = &rt->func[rt->fcnt];
		pb->cur->argc = 0;

		ssCheck(ssNextToken(pb));

		if (pb->type == SS_EOF_TYPE)
			return 0;

		if (pb->type != SS_NAME_TYPE || strcmp(pb->name, "function"))
			ssError("%d: Expected function declaration\n", pb->line);

		ssCheck(ssParseFunc(pb));
		assert((size_t) (pb->wp - p) <= n);
	}
}

static int ssLoad(struct SsRuntime* rt, struct SsParseBuffer* pb, char* p, size_t n, const char* name)
{
	FILE* fd;
	size_t m;

	if ((fd = fopen(name, "rb")) == NULL)
		return -1;

	fseek(fd, 0, SEEK_END);

	if ((m = ftell(fd)) >= n)
		ssError("File (%u) is too large for buffer (%u)\n", m, n);

	rewind(fd);
	fread(p, m, 1, fd);
	p[m] = 0;

	return ssParse(rt, pb, p, n);
}

#define ssCall1(rt,name,a0) \
	do { \
		float v[SS_MAX_NUM_VARS] = {(a0)}; \
		ssCall((rt), (name), v); \
		(a0) = v[0]; \
	} while (0)

#define ssCall2(rt,name,a0,a1) \
	do { \
		float v[SS_MAX_NUM_VARS] = {(a0), (a1)}; \
		ssCall((rt), (name), v); \
		(a0) = v[0]; (a1) = v[1]; \
	} while (0)

#define ssCall3(rt,name,a0,a1,a2) \
	do { \
		float v[SS_MAX_NUM_VARS] = {(a0), (a1), (a2)}; \
		ssCall((rt), (name), v); \
		(a0) = v[0], (a1) = v[1], (a2) = v[2]; \
	} while (0)

#define ssCall4(rt,name,a0,a1,a2,a3) \
	do { \
		float v[SS_MAX_NUM_VARS] = {(a0), (a1), (a2), (a3)}; \
		ssCall((rt), (name), v); \
		(a0) = v[0], (a1) = v[1], (a2) = v[2], (a3) = v[3]; \
	} while (0)

#define ssCall5(rt,name,a0,a1,a2,a3,a4) \
	do { \
		float v[SS_MAX_NUM_VARS] = {(a0), (a1), (a2), (a3), (a4)}; \
		ssCall((rt), (name), v); \
		(a0) = v[0], (a1) = v[1], (a2) = v[2], (a3) = v[3], (a4) = v[4]; \
	} while (0)

#define ssCall6(rt,name,a0,a1,a2,a3,a4,a5) \
	do { \
		float v[SS_MAX_NUM_VARS] = {(a0), (a1), (a2), (a3), (a4), (a5)}; \
		ssCall((rt), (name), v); \
		(a0) = v[0], (a1) = v[1], (a2) = v[2], (a3) = v[3], (a4) = v[4], (a5) = v[5]; \
	} while (0)

#define ssCall7(rt,name,a0,a1,a2,a3,a4,a5,a6) \
	do { \
		float v[SS_MAX_NUM_VARS] = {(a0), (a1), (a2), (a3), (a4), (a5), (a6)}; \
		ssCall((rt), (name), v); \
		(a0) = v[0], (a1) = v[1], (a2) = v[2], (a3) = v[3], (a4) = v[4], (a5) = v[5], (a6) = v[6]; \
	} while (0)

#define ssCall8(rt,name,a0,a1,a2,a3,a4,a5,a6,a7) \
	do { \
		float v[SS_MAX_NUM_VARS] = {(a0), (a1), (a2), (a3), (a4), (a5), (a6), (a7)}; \
		ssCall((rt), (name), v); \
		(a0) = v[0], (a1) = v[1], (a2) = v[2], (a3) = v[3], (a4) = v[4], (a5) = v[5], (a6) = v[6], (a7) = v[7]; \
	} while (0)

#define ssCall9(rt,name,a0,a1,a2,a3,a4,a5,a6,a7,a8) \
	do { \
		float v[SS_MAX_NUM_VARS] = {(a0), (a1), (a2), (a3), (a4), (a5), (a6), (a7), (a8)}; \
		ssCall((rt), (name), v); \
		(a0) = v[0], (a1) = v[1], (a2) = v[2], (a3) = v[3], (a4) = v[4], (a5) = v[5], (a6) = v[6], (a7) = v[7], (a8) = v[8]; \
	} while (0)

#define ssCall10(rt,name,a0,a1,a2,a3,a4,a5,a6,a7,a8,a9) \
	do { \
		float v[SS_MAX_NUM_VARS] = {(a0), (a1), (a2), (a3), (a4), (a5), (a6), (a7), (a8), (a9)}; \
		ssCall((rt), (name), v); \
		(a0) = v[0], (a1) = v[1], (a2) = v[2], (a3) = v[3], (a4) = v[4], (a5) = v[5], (a6) = v[6], (a7) = v[7], (a8) = v[8], (a9) = v[9]; \
	} while (0)

static int ssCall(const struct SsRuntime* rt, const char* name, float* vars)
{
	float stack[SS_MAX_NUM_STACK], *sp = &stack[SS_MAX_NUM_STACK], eax;
	const char* code;
	int i, op;

	for (i = 0; i < rt->fcnt; ++i)
		if (!strcmp(rt->func[i].name, name))
			break;

	if (i == rt->fcnt)
		return -1;

	code = rt->func[i].code;

	while ((op = *code++) != SS_STOP_OP) {
#ifdef ssTrace
		float tmp;
		switch (op) {
		case SS_PUSH_OP: ssTrace("  push\n"); break;
		case SS_CONST_OP:
			memcpy(&tmp, code, 4);
			ssTrace("  const %.02f\n", tmp);
			break;

		case SS_LOAD_OP: ssTrace("  load 0x%x\n", *code); break;
		case SS_STORE_OP: ssTrace("  store 0x%x\n", *code); break;
		case SS_SEL_OP:
			ssTrace("  sel %.02f %.02f %.02f\n", sp[1], sp[0], eax);
			break;

		case SS_MIN_OP: ssTrace("  min %.02f %.02f\n", *sp, eax); break;
		case SS_MAX_OP: ssTrace("  max %.02f %.02f\n", *sp, eax); break;
		case SS_CLAMP_OP: ssTrace("  clamp %.02f %.02f %.02f\n", sp[1], sp[0], eax); break;
		case SS_SATURATE_OP: ssTrace("  saturate %.02f\n", eax); break;
		case SS_ADD_OP: ssTrace("  add %.02f %.02f\n", *sp, eax); break;
		case SS_SUB_OP: ssTrace("  sub %.02f %.02f\n", *sp, eax); break;
		case SS_MUL_OP: ssTrace("  mul %.02f %.02f\n", *sp, eax); break;
		case SS_DIV_OP: ssTrace("  div %.02f %.02f\n", *sp, eax); break;
		case SS_FLOOR_OP: ssTrace("  floor %.02f\n", eax); break;
		case SS_CEIL_OP: ssTrace("  ceil %.02f\n", eax); break;
		case SS_ABS_OP: ssTrace("  abs %.02f\n", eax); break;
		case SS_SQR_OP: ssTrace("  sqr %.02f\n", eax); break;
		case SS_SQRT_OP: ssTrace("  sqrt %.02f\n", eax); break;
		case SS_POW_OP: ssTrace("  pow %.02f %.02f\n", *sp, eax); break;
		case SS_EXP_OP: ssTrace("  exp %.02f\n", eax); break;
		case SS_SIN_OP: ssTrace("  sin %.02f\n", eax); break;
		case SS_COS_OP: ssTrace("  cos %.02f\n", eax); break;
		case SS_ASIN_OP: ssTrace("  asin %.02f\n", eax); break;
		case SS_ACOS_OP: ssTrace("  acos %.02f\n", eax); break;
		}
#endif

		switch (op) {
		case SS_PUSH_OP: *(--sp) = eax; break;
		case SS_CONST_OP:
			memcpy(&eax, code, sizeof(eax));
			code += sizeof(eax);
			break;

		case SS_LOAD_OP: eax = vars[(int) *code++]; break;
		case SS_STORE_OP: vars[(int) *code++] = eax; break;
		case SS_SEL_OP: eax = sp[1] >= 0.f ? sp[0] : eax; sp += 2; break;
		case SS_MIN_OP: eax = *sp < eax ? *sp : eax; sp++; break;
		case SS_MAX_OP: eax = *sp < eax ? eax : *sp; sp++; break;
		case SS_CLAMP_OP: eax = sp[1] >= sp[0] ? sp[1] <= eax ? sp[1] : eax : sp[0]; sp += 2; break;
		case SS_SATURATE_OP: eax = eax >= 0.f ? eax <= 1.f ? eax : 1.f : 0.f; break;
		case SS_ADD_OP: eax = *sp++ + eax; break;
		case SS_SUB_OP: eax = *sp++ - eax; break;
		case SS_MUL_OP: eax = *sp++ * eax; break;
		case SS_DIV_OP: eax = *sp++ / eax; break;
		case SS_ABS_OP: eax = fabsf(eax); break;
		case SS_FLOOR_OP: eax = floorf(eax); break;
		case SS_CEIL_OP: eax = ceilf(eax); break;
		case SS_SQR_OP: eax = eax * eax; break;
		case SS_SQRT_OP: eax = sqrtf(eax); break;
		case SS_POW_OP: eax = powf(*sp++, eax); break;
		case SS_EXP_OP: eax = expf(eax); break;
		case SS_SIN_OP: eax = sinf(eax); break;
		case SS_COS_OP: eax = cosf(eax); break;
		case SS_ASIN_OP: eax = asinf(eax); break;
		case SS_ACOS_OP: eax = acosf(eax); break;
		}
	}

	if (sp != &stack[SS_MAX_NUM_STACK])
		ssError("Unbalanced stack pointer after call\n");

	return 0;
}
