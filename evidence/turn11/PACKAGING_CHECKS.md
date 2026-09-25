# Packaging check correction

The first full-inventory verifier rejected the report because the canonical-hash header line ended in Markdown hard-break spaces while the inherited verifier requires that field to end at the closing backtick. The report field was corrected to its required syntax; canonical and literal hashes were recomputed. No verifier was weakened, no native source changed, and no execution result was changed. The successful package checks follow this correction.
