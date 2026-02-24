# P8-W6 Decisions

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Key decisions

1. **WCSLIB for projection math** — All celestial projection calculations
   (SIN, TAN, ARC, SFL, and ~30 other FITS-standard projection types) are
   delegated to WCSLIB rather than reimplemented. WCSLIB is the reference
   implementation for FITS WCS and provides tested, standards-compliant
   projection code.

2. **Base Coordinate class with virtual pixel/world transforms** — Coordinates
   use an inheritance hierarchy with abstract base class, unlike measures which
   use variants. This is appropriate because coordinate types have genuinely
   polymorphic interfaces (different implementations of pixel<->world transforms)
   and are stored heterogeneously in CoordinateSystem via unique_ptr.

## Tradeoffs accepted

1. WCSLIB dependency adds ~500 KB of compiled C code and requires pkg-config
   for detection. Accepted because reimplementing ~30 projection types would
   be far larger and more error-prone.

2. Virtual dispatch for coordinates adds a vtable pointer per coordinate object.
   Accepted because coordinates are few in number (typically 2-5 per image) and
   the polymorphic interface is genuinely needed.
