#!/usr/bin/env python3
"""
build_page.py — inline cx-dump traces into lab_template.html.

  build_page.py -o OUT.html trace1.jsonl [trace2.jsonl ...]

Each trace may hold several jobs (one cx-dump process). Jobs are matched to the
official vectors below by (key, input); a job with a full trace (program/iter
events) becomes a browsable vector, a job with only job/job_end becomes a
result-only row.
"""
import json, os, sys

OFFICIAL = [
    {"name": "1a", "key": "test key 000", "input": "This is a test",
     "v1": "639183aae1bf4c9a35884cb46b09cad9175f04efd7684e7262a0ac1c2f0b4e3f", "v2": "22ec6b861b3eb23686b2efbad69513c967ecfce80983df66c9c5b4fbfb4cdb6f"},
    {"name": "1b", "key": "test key 000", "input": "Lorem ipsum dolor sit amet",
     "v1": "300a0adb47603dedb42228ccb2b211104f4da45af709cd7547cd049e9489c969", "v2": "9e2c772c12fd48f93c14c97fdc89d556264d9100597023f44d9163e279012ecf"},
    {"name": "1c", "key": "test key 000", "input": "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua",
     "v1": "c36d4ed4191e617309867ed66a443be4075014e2b061bcdaf9ce7b721d2b77a8", "v2": "4d6b063a1a603751d525f18a171336a4002f2f06df6c17e4b25fe17e17796e42"},
    {"name": "1d", "key": "test key 001", "input": "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua",
     "v1": "e9ff4503201c0c2cca26d285c93ae883f9b1d30c9eb240b820756f2d5a7905fc", "v2": "97024134686ce27d362ea8d86d8ef16483ac272abdabd46ef13359400777fe5e"},
    {"name": "1e", "key": "test key 001",
     "input": "0x0b0b98bea7e805e0010a2126d287a2a0cc833d312cb786385a7c2f9de69d25537f584a9bc9977b00000000666fd8753bf61a8631f12984e3fd44f4014eca629276817b56f32e9b68bd82f416",
     "v1": "c56414121acda1713c2f2a819d8ae38aed7c80c35c2a769298d34f03833cd5f1", "v2": "c8e92c5f7c1946fecf06bc382b92e3111da38ee3e6a5ad90704e1a9d8aaf6e76"},
    {"name": "1f", "key": "0x7797373ea4633194640bf8d8c3b66724d6aa7bd2dc20e009df2f8f1710abe8",
     "input": "0x1010e1eaf8cf067b37b5f0ee031ab23ed1755e090a3af4415830145853e2be3e1f6821fed84dae58d00e00da5214d6c1f2d0622e0abd51f9373d04e0b0f8e6d6514d90689721c4aac5a9bb0d",
     "v1": "78af2a1864c42abce36d2e8983e13df99b2af0ce1362999af09fab004d4435a8", "v2": None},
]

FINDINGS = [
    {"where": "abacus.c compare_magnitude", "defect": "Digit walk stopped at min(max_exp_a, max_exp_b), i.e. after the leading digit; two numbers with the same digit count and same leading digit compared EQUAL. Cascades into abacus_sub (zero result / wrong sign), abacus_div (binary search recurses to a stack overflow), abacus_sqrt (err 13 / zero).", "repro": "A = 0xDEADBEEFCAFEBABE: abacus_compare(A+A, 2^64) -> 0; abacus_div(A*B, 2^64) -> SIGSEGV", "status": "patched in the lab copy (patch #1); harness runs on the patched copy"},
    {"where": "abacus.c abacus_add / abacus_sub / abacus_mul; abacus_div; abacus_gcd.c abacus_sqrt", "defect": "Fast paths convert both operands to uint64_t and use the CPU's native + - * / % and a native Newton loop. For 64-bit operands the library defers to the machine.", "repro": "grep abacus_to_uint64 in those functions", "status": "add/sub/mul shortcuts compiled out with -DCX_PURE_GEOMETRIC (patch #2); selftest passes on both builds; div/sqrt not used by the harness"},
    {"where": "abacus_div (bisection path)", "defect": "For dividends above 2^64 the fallback bisection calls abacus_div recursively on mid/2 and overflows the stack.", "repro": "abacus_div(2^128, 2^64)", "status": "not used; harness does long division from compare/sub/shift"},
    {"where": "abacus_sqrt", "defect": "Errors (13) or returns 0 for inputs above 2^64.", "repro": "abacus_sqrt((2^64-1)^2) -> 0", "status": "not used; harness does digit-by-digit root"},
    {"where": "abacus_convert_base", "defect": "Conversion to base 2 loses low bits.", "repro": "0xDEADBEEFCAFEBABE -> base 2 -> 0xDEADBEEFCAFEB800", "status": "avoided; abacus_from_uint64(x, 2) is exact"},
    {"where": "abacus_mul on fractional beads", "defect": "Wrong product for numbers with negative weight exponents.", "repro": "(pi/2)^2 via shift_right(52) then mul -> 1", "status": "avoided; doubles use integer mantissas"},
    {"where": "abacus_shift_left / abacus_mul", "defect": "max_exponent not updated (min_exponent can exceed max_exponent); zero beads stripped, leaving holes.", "repro": "shift_left(1, 4): exp[4..0]", "status": "harness reads beads by weight_exponent, never by index or metadata"},
    {"where": "abacus_shift_right", "defect": "Keeps the shifted-out digits as fractional beads (fixed-point), so it is not an integer shift.", "repro": "shift_right(A, 4) has beads at weight -4..-1", "status": "harness reads integer digits only"},
    {"where": "include/math.h", "defect": "Ships a math.h that shadows libc's when the include dir is on the search path (sqrt, isfinite vanish; C++ <cmath> fails).", "repro": "cc -I crystalline/include foo.c with #include <math.h>", "status": "harness keeps the include dir off the RandomX build"},
]

# Traces written before the counters were split into crystalline/native also counted the
# trace hook's own scratchpad-fingerprint Blake2b (run in bypass mode) as ops. Measured with
# the split counters; subtracted from those older traces so "ops this job" = abacus ops only.
FINGERPRINT_BYPASS_OPS = 22396936   # measured 2026-09-12 with split counters; = 16384 Blake2b compressions x 1367 hook calls + 8

def load(path):
    evs = [json.loads(l) for l in open(path) if l.strip()]
    jobs, curjob, pre = [], None, []
    for e in evs:
        if e["ev"] == "job": curjob = {"job": e, "events": []}; jobs.append(curjob)
        elif e["ev"] == "counters":
            for j in jobs: j["events"].append(e)
        elif curjob is None: pre.append(e)
        else: curjob["events"].append(e)
    for j in jobs:
        if "counting" not in j["job"] and FINGERPRINT_BYPASS_OPS:
            after_fp = False
            for e in j["events"]:
                if e["ev"] == "scratchpad": after_fp = True; continue
                if after_fp and "ops" in e: e["ops"] -= FINGERPRINT_BYPASS_OPS
                if e["ev"] == "job_end":
                    e["ops_this_job_raw"] = e["ops_this_job"]; e["ops_this_job"] -= FINGERPRINT_BYPASS_OPS
                    e["ops_note"] = f"{FINGERPRINT_BYPASS_OPS:,} native-mode ops of the trace hook's own fingerprint hash excluded"
        j["events"] = pre + [j["job"]] + j["events"]
    return jobs

def main():
    args = sys.argv[1:]; out = "RandomX-formula-lab.html"
    if "-o" in args: i = args.index("-o"); out = args[i + 1]; del args[i:i + 2]
    jobs = [j for p in args for j in load(p)]
    vectors = []
    for o in OFFICIAL:
        runs = {}
        for j in jobs:
            jb = j["job"]
            if jb["key"] == o["key"] and jb["input"] == o["input"]:
                runs["v2" if jb["v2"] else "v1"] = j["events"]
        if runs:
            vectors.append({"name": o["name"], "key": o["key"], "input": o["input"], "official": {"v1": o["v1"], "v2": o["v2"]}, "runs": runs})
    if not vectors: sys.exit("no jobs matched the official vectors")
    tpl = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "lab_template.html")).read()
    data = json.dumps({"vectors": vectors, "findings": FINDINGS}).replace("</", "<\\/")
    open(out, "w").write(tpl.replace("/*__CX_DATA__*/", data))
    print(f"wrote {out}: {len(vectors)} vectors, {len(jobs)} jobs, {os.path.getsize(out)//1024} KB")

if __name__ == "__main__": main()
