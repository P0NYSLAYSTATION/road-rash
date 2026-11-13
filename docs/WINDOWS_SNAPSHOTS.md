# Windows authoritative snapshots

Work in progress. The user selected host authority, conditional on collision
simulation covering every player independently of the host rendered view.

The existing Git history begins with input-timeline rollback; there is no earlier
native snapshot implementation to restore. Original packet traces remain useful
for cadence and gameplay state, but this implementation uses a modern authoritative
host rather than original per-player/distributed AI ownership.

Acceptance requires remote-player scenery, walls, rider/car/pedestrian contacts,
police, crashes/remounts, split roads and active windows outside the host camera;
only the host may publish authoritative state or race results. Clients predict
locally and reconcile unacknowledged controls to state snapshots. Offline oracle
semantics must remain unchanged. Native Windows capture runs only E:/RR/ROADRASH.EXE in E:/RR. Its working
AWEMAN32.DLL is backed up, extended with an additive capture entry, and restored
with checksum verification. Private-copy and reference-runtime launch paths are
removed. The original DLL sections/exports remain unchanged in the instrumented
version. Capture settings use a temporary user registry key because this game's
KEY_ALL_ACCESS request fails with access denied when launched by the command
runner against the machine key. No build or remote operation is authorized by this document.


Current verification: `snapshot-test` checks eight-human portable state
round-trips, corruption rejection without live mutation, continued simulation
without RNG reanchoring, repeated client corrections and rejection of forged
client world state at the host. A headless synthetic test places the remote
human on another road at sample 600 while the host remains near sample 50;
boxed scenery contacts work without any rendered frame. An explicit E:/RR test
builds a real two-human/full-AI Napa race, advances its countdown and 180 racing
ticks, restores riders/cars/pedestrians/ring cursors, and matches state hashes for
120 further ticks. Its full snapshot is 27,089 bytes. These are bounded tests,
not proof of every object/contact/branch. Traffic cross-road ownership/window
filtering and server-level snapshot authorization still require completion.

The codec now includes mutable schedule order, countdown flags, ground slices,
ring walkers/terrain iterators/DZHR marks, per-seat traffic timers/windows, and
consumed pedestrian records. Immutable section/asset pointers are reconstructed
locally; snapshot application consumes no gameplay RNG. Host-authority setup is
idempotent and is initialized by the shared factory before the session starts.
