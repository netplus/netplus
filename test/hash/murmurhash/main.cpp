#include <netp.hpp>
#include <wawo/security/murmurhash.hpp>

int main(int argc, char** argv) {

	int i;
	for (i = 0; i < 0; i++) {
	
	}

	printf("ii: %d\n", i);

	char byte[][10] = {
		{0,1,2,3,4,5,6,7,8,9},
		{1,0,2,3,4,5,6,7,8,9},
		{1,0,3,2,4,5,6,7,8,9},
		{1,1,2,3,4,5,6,7,8,9},
		{9,8,7,6,5,4,3,2,1,0}
	};

	for (auto s : byte) {
		uint32_t i;
		netp::security::MurmurHash3_x86_32(s, 10, 0, &i);
		printf("i: %u\n", i);
	}
	printf("\n");

	char byte2[][9] = {
		{0,1,2,3,4,5,6,7,8},
		{1,0,2,3,4,5,6,7,8},
		{1,0,3,2,4,5,6,7,8},
		{1,1,2,3,4,5,6,7,8},
		{9,8,7,6,5,4,3,2,1}
	};

	for (auto s : byte2) {
		uint32_t i;
		netp::security::MurmurHash3_x86_32(s, 10, 1, &i);
		printf("i: %u\n", i);
	}
	return 0;
}