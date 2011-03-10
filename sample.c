
#define ssTrace printf
#include "scriptshader.h"

struct SsRuntime g_rt;
struct SsParseBuffer g_pb;
char g_buf[0x10000];

void calcCircleArea()
{
	float area = NAN, radius = 2;

	printf("calcCircleArea()\n");
	printf(" before: area=%.02f radius=%.02f\n", area, radius);

	ssCall2(&g_rt, "calcCircleArea", area, radius);

	printf(" after: area=%.02f radius=%.02f\n", area, radius);
}

void calcSectorArea()
{
	float area = NAN, radius = 2, angle = M_PI;

	printf("calcSectorArea()\n");
	printf(" before: area=%.02f radius=%.02f angle=%.02f\n", area, radius, angle);

	ssCall3(&g_rt, "calcSectorArea", area, radius, angle);

	printf(" after: area=%.02f radius=%.02f angle=%.02f\n", area, radius, angle);
}

int main(int argc, char* argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: sample <file>\n");
		return 1;
	}

	if (ssLoad(&g_rt, &g_pb, g_buf, sizeof(g_buf), argv[1]) < 0) {
		fprintf(stderr, "Failed to load %s\n", argv[1]);
		return 1;
	}

	calcCircleArea();
	calcSectorArea();

	return 0;
}
