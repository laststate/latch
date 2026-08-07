# Safe-state integration boundary

Latch deliberately does not define an AUV safe state. The correct response to a
failure depends on depth, buoyancy architecture, propulsion state, navigation quality,
battery condition, tether/acoustic availability, operating area and mission rules.

The vehicle safety controller may use its own independently validated logic to enter a
product-specific state such as stop propulsion, surface, drop weight, hold depth,
abort mission or activate recovery aids. Latch may record that decision with a
breadcrumb/event, but Latch must not be the authority that decides it.

For integration, define a table mapping each safety-relevant detector to: detecting
component, independent fallback, commanded safe state, maximum response time, Latch
evidence emitted, and physical HIL test. Keep that table with the vehicle safety case,
not as a generic claim in this library.
