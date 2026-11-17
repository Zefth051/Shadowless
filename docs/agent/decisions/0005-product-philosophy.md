# 0005: Product philosophy is a permanent design constraint

Status: accepted

Decision: `docs/PRODUCT_PHILOSOPHY.md` is the single source of truth for
product behavior (install once, zero friction, native authorization, graceful
degradation, no repeated setup). Agent instruction files link to it rather
than copy it.

Consequences: features that need terminal steps, mandatory manual setup or
repeated permission prompts are redesigned, not documented. Security and
privacy are never traded for convenience.
