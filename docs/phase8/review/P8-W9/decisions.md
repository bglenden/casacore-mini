# P8-W9 Decisions

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Key decisions

1. **FITS header synthesis utilities** — FITSCoordinateUtil provides
   bidirectional conversion between CoordinateSystem objects and FITS WCS
   header keywords (CTYPE, CRVAL, CDELT, CRPIX, PC matrix, etc.). This
   enables reading and writing FITS files with proper coordinate metadata
   without depending on a full FITS I/O library.

2. **Gaussian deconvolution in pixel/world frames** — GaussianConvert handles
   the coordinate-dependent transformation of Gaussian source parameters
   (major axis, minor axis, position angle) between pixel and world coordinate
   frames. This accounts for the non-uniform pixel scale and rotation of the
   DirectionCoordinate at the source position.

## Tradeoffs accepted

1. FITS header synthesis does not cover all FITS WCS extensions (e.g., SIP
   distortion, TPV). Only standard FITS WCS Paper I-III keywords are supported.
   This covers the vast majority of radio astronomy use cases.

2. Gaussian deconvolution uses a local linear approximation of the coordinate
   transform at the source position. This is accurate for sources much smaller
   than the field of view, which is the standard case in radio astronomy.
