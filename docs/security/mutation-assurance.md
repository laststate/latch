# Mutation-testing assurance

Latch uses two mutation layers:

1. `tools/mutation_smoke.py` — fast PR smoke mutants for a few high-risk predicates.
2. `tools/mutation_campaign.py` — broad deterministic critical-runtime campaign.

The broad campaign currently spans 40 intentionally non-equivalent mutations across:

- crypto provider assurance and KAT enforcement;
- security key/policy validation and authentication failure behavior;
- secure storage integrity;
- flash mirror and wear-level integrity;
- NOR programming rules;
- emergency/critical spool reservation and spool CRC handling;
- HTTP/MQTT classification;
- stream ACK/CRC parsing;
- varint overflow rejection.

CI rejects surviving valid mutants and also enforces a minimum mutation score. Compilation-invalid
mutants are reported separately and excluded from the score rather than counted as kills.
The JSON report is uploaded as a CI artifact.

The 2026-08-07 qualification run killed 40/40 valid mutants (100%, zero invalid). The first broad run
had four survivors; tests were strengthened until those same mutants were killed instead of deleting
or weakening them.
