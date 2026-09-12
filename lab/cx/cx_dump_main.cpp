// cx-dump — compute RandomX hashes through the crystalline-routed reference and
// emit the per-step trace the lab page renders (newline-delimited JSON).
//
//   cx-dump -k <key> -i <input> [-i <input> ...] [-v 1|2|both] [-o out.jsonl] [-name 1a ...]
//
// Inputs/keys may be hex with a 0x prefix (Monero hashing blobs). One process
// computes every (input, version) pair, so the Argon2 cache for the key is
// filled (and crystalline-verified) once.
#include "randomx.h"
#include "cx64.h"
#include "cx_dump.h"
#include "cx_aes.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

extern "C" const uint32_t randomx_aes_lut_enc[4][256];
extern "C" const uint32_t randomx_aes_lut_dec[4][256];

static std::vector<uint8_t> parse(const std::string &s) {
	std::vector<uint8_t> v;
	if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		for (size_t i = 2; i + 1 < s.size(); i += 2) v.push_back((uint8_t)strtoul(s.substr(i, 2).c_str(), nullptr, 16));
	} else v.assign(s.begin(), s.end());
	return v;
}
static double now() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

int main(int argc, char **argv) {
	std::string key, ver = "1", outpath = "-";
	std::vector<std::string> inputs, names;
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		if (a == "-k" && i + 1 < argc) key = argv[++i];
		else if (a == "-i" && i + 1 < argc) inputs.push_back(argv[++i]);
		else if (a == "-name" && i + 1 < argc) names.push_back(argv[++i]);
		else if (a == "-v" && i + 1 < argc) ver = argv[++i];
		else if (a == "-o" && i + 1 < argc) outpath = argv[++i];
		else { fprintf(stderr, "usage: cx-dump -k <key> -i <input> [-i ...] [-v 1|2|both] [-o file] [-name ...]\n"); return 2; }
	}
	if (key.empty() || inputs.empty()) { fprintf(stderr, "need -k and at least one -i\n"); return 2; }
	std::vector<int> versions; if (ver == "1") versions = {0}; else if (ver == "2") versions = {1}; else versions = {0, 1};

	cx_init();
	const char *m = getenv("CX_MODE");
	cx_set_mode(!m ? CX_STRICT : !strcmp(m, "off") ? CX_OFF : !strcmp(m, "check") ? CX_CHECK : CX_STRICT);
	cx_dump_open(outpath.c_str());

	uint64_t aes_ops = 0; int aes_bad = cx_aes_verify(randomx_aes_lut_enc, randomx_aes_lut_dec, &aes_ops);
	cx_dump_begin("aes_tables"); cx_dump_int("mismatching_entries", aes_bad); cx_dump_int("entries", 2048); cx_dump_int("ops_used", (long long)aes_ops); cx_dump_end();

	std::vector<uint8_t> kb = parse(key);
	randomx_cache *cache = randomx_alloc_cache(RANDOMX_FLAG_DEFAULT);
	double t0 = now();
	randomx_init_cache(cache, kb.data(), kb.size());
	cx_dump_begin("cache_ready"); cx_dump_str("key", key.c_str()); cx_dump_hex("key_hex", kb.data(), kb.size()); cx_dump_int("seconds", (long long)(now() - t0)); cx_dump_end();

	int rc = 0;
	for (size_t ii = 0; ii < inputs.size(); ii++) for (int v2 : versions) {
		std::vector<uint8_t> in = parse(inputs[ii]);
		randomx_flags flags = RANDOMX_FLAG_DEFAULT;
		if (v2) flags = (randomx_flags)(flags | RANDOMX_FLAG_V2);
		randomx_vm *vm = randomx_create_vm(flags, cache, nullptr);
		uint64_t ops0 = cx_total_calls(), mm0 = cx_total_mismatches(), nat0 = cx_total_native(); double t1 = now();
		cx_dump_reset_items();

		cx_dump_begin("job");
		cx_dump_str("name", ii < names.size() ? names[ii].c_str() : "");
		cx_dump_str("key", key.c_str()); cx_dump_hex("key_hex", kb.data(), kb.size());
		cx_dump_str("input", inputs[ii].c_str()); cx_dump_hex("input_hex", in.data(), in.size());
		cx_dump_int("v2", v2); cx_dump_str("vm", "interpreter, light mode, soft AES, crystalline strict"); cx_dump_str("counting", "crystalline-only");
		cx_dump_end();

		uint8_t hash[RANDOMX_HASH_SIZE];
		randomx_calculate_hash(vm, in.data(), in.size(), hash);

		cx_dump_begin("job_end"); cx_dump_hex("R", hash, sizeof hash); cx_dump_int("v2", v2);
		cx_dump_int("ops_this_job", (long long)(cx_total_calls() - ops0)); cx_dump_int("mismatches_this_job", (long long)(cx_total_mismatches() - mm0)); cx_dump_int("native_ops_this_job", (long long)(cx_total_native() - nat0));
		cx_dump_int("seconds", (long long)(now() - t1)); cx_dump_end();
		fprintf(stderr, "%s v%d R = ", ii < names.size() ? names[ii].c_str() : inputs[ii].c_str(), v2 ? 2 : 1);
		for (int i = 0; i < 32; i++) fprintf(stderr, "%02x", hash[i]);
		fprintf(stderr, "  (%llu crystalline ops, %llu mismatches, %llu native bypass ops, %.0fs)\n", (unsigned long long)(cx_total_calls() - ops0), (unsigned long long)(cx_total_mismatches() - mm0), (unsigned long long)(cx_total_native() - nat0), now() - t1);
		randomx_destroy_vm(vm);
	}
	cx_dump_counters();
	cx_dump_close();
	randomx_release_cache(cache);
	cx_report();
	if (cx_total_mismatches()) rc = 1;
	return rc;
}
