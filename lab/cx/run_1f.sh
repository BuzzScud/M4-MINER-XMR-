#!/bin/zsh
# Vector 1f alone: 31-byte key (tests.cpp passes N-1 bytes of its 32-byte array), full Argon2 through crystalline.
cd "${0:A:h:h}/RandomX-cx-dump" || exit 1
echo "== 1f strict run started $(date +%H:%M)"
(time CX_MODE=strict ./cx-dump -k 0x7797373ea4633194640bf8d8c3b66724d6aa7bd2dc20e009df2f8f1710abe8 \
   -i 0x1010e1eaf8cf067b37b5f0ee031ab23ed1755e090a3af4415830145853e2be3e1f6821fed84dae58d00e00da5214d6c1f2d0622e0abd51f9373d04e0b0f8e6d6514d90689721c4aac5a9bb0d -name 1f \
   -v 1 -o ../traces/key1f_full.jsonl) 2>&1 | grep -E 'R =|mismatch|real'
echo "== 1F DONE $(date +%H:%M)  (official 78af2a1864c42abce36d2e8983e13df99b2af0ce1362999af09fab004d4435a8)"
