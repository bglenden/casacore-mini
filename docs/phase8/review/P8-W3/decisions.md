# P8-W3 Decisions

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Key decisions

1. **ERFA for astronomical transforms** — All epoch, position, and direction
   conversions delegate to ERFA (Essential Routines for Fundamental Astronomy),
   the open-source successor to IAU SOFA. This provides IAU-standard accuracy
   without reimplementing complex astronomical algorithms.

2. **J2000 as intermediate for direction conversions** — Direction frame
   conversions route through J2000 as a hub frame. Converting from frame A to
   frame B goes A -> J2000 -> B. This reduces the number of conversion paths
   from O(n^2) to O(n) while maintaining full accuracy through ERFA's
   precession/nutation models.

## Tradeoffs accepted

1. J2000 hub routing means some conversions (e.g., galactic to ecliptic)
   perform two ERFA calls instead of a potential direct path. The accuracy
   impact is negligible and the implementation simplification is significant.

2. ERFA dependency adds ~200 KB of compiled C code. Accepted because the
   alternative (reimplementing IAU standard algorithms) would be far larger
   and error-prone.
