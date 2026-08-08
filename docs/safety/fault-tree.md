# Fault-tree starter: loss of diagnostic evidence

Top event: **a mission-critical incident occurs and no trustworthy diagnostic evidence
is recoverable after reboot/retrieval**.

Contributing branches to analyze in the product safety case:

1. Capture unavailable: fault entry not installed, nested fault, corrupt stack,
   retained region overwritten, watchdog reset before bounded capture completes.
2. Persistence unavailable: spool exhausted, flash write/erase failure, power loss at
   an unqualified cut point, wrong geometry, endurance exhausted, corruption/ECC loss.
3. Authenticity unavailable: key unavailable, nonce/entropy failure, wrong key ID,
   provisioning state lost or secure element inaccessible.
4. Delivery unavailable: transport offline longer than local retention, framing
   failure, collector rejects data, collector ACKs before durable commit.
5. Interpretation unavailable: wrong build ID/symbols, incompatible decoder, missing
   boot/session identity or release evidence.

Required design rule: no single Latch failure may be allowed to command or inhibit a
vehicle safety action. Safety mechanisms operate independently; Latch observes them.
