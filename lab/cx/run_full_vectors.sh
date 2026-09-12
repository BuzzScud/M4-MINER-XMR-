#!/bin/zsh
# Full strict run: all six official vectors, every Argon2 block through crystalline.
# Produces traces/key000_full.jsonl, key001_full.jsonl, key1f_full.jsonl (~3 h on the M4).
# Usage: nohup ./run_full_vectors.sh > ../traces/full_run.log 2>&1 &
set -u
LAB="${0:A:h:h}"
cd "$LAB/RandomX-cx-dump" || exit 1
until [ -f ../traces/.build_ok ]; do sleep 5; done; sleep 10
echo "== FULL strict run, Argon2 every block (started $(date +%H:%M))"
(time CX_MODE=strict ./cx-dump -k "test key 000" -i "This is a test" -name 1a -i "Lorem ipsum dolor sit amet" -name 1b \
   -i "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua" -name 1c -v both -o ../traces/key000_full.jsonl) 2>&1 | grep -E 'R =|mismatch|real'
echo "-- key000 done $(date +%H:%M)"
(time CX_MODE=strict ./cx-dump -k "test key 001" -i "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua" -name 1d \
   -i 0x0b0b98bea7e805e0010a2126d287a2a0cc833d312cb786385a7c2f9de69d25537f584a9bc9977b00000000666fd8753bf61a8631f12984e3fd44f4014eca629276817b56f32e9b68bd82f416 -name 1e \
   -v both -o ../traces/key001_full.jsonl) 2>&1 | grep -E 'R =|mismatch|real'
echo "-- key001 done $(date +%H:%M)"
(time CX_MODE=strict ./cx-dump -k 0x7797373ea4633194640bf8d8c3b66724d6aa7bd2dc20e009df2f8f1710abe8 \
   -i 0x1010e1eaf8cf067b37b5f0ee031ab23ed1755e090a3af4415830145853e2be3e1f6821fed84dae58d00e00da5214d6c1f2d0622e0abd51f9373d04e0b0f8e6d6514d90689721c4aac5a9bb0d -name 1f \
   -v 1 -o ../traces/key1f_full.jsonl) 2>&1 | grep -E 'R =|mismatch|real'
echo "== FULL RUN DONE $(date +%H:%M)"
ls -la ../traces/*_full.jsonl
