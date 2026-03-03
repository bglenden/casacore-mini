// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/lattice_array.hpp"
#include "casacore_mini/lattice_shape.hpp"
#include "casacore_mini/record.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Lattice-coordinate (LC) region hierarchy for pixel-domain regions.
///
/// An LcRegion describes a region in pixel coordinates. It has a bounding box
/// (Slicer) within a lattice shape, and optionally a boolean mask within
/// that bounding box. Compound regions combine sub-regions via set algebra.
/// @ingroup images
/// @addtogroup images
/// @{

// ── LcRegion (abstract base) ─────────────────────────────────────────

///
/// Abstract base class for lattice-coordinate (pixel-domain) regions.
///
///
///
///
///
/// An `LcRegion` describes a region expressed in pixel coordinates within a
/// lattice of known shape.  Every `LcRegion` has:
///
/// - A `lattice_shape` — the shape of the lattice it lives in.
/// - A `bounding_box` (a `Slicer`) — the tightest axis-aligned box enclosing
///   the region within the lattice.
/// - An optional boolean mask (`get_mask()`) over the bounding box:
///   `true` = pixel included, `false` = pixel excluded.  `has_mask()` returns
///   `false` for box regions where all pixels in the bounding box are included.
///
/// Subclasses are expected to implement `type()`, `clone()`, `to_record()`,
/// and (if they have a non-trivial mask) `has_mask()` and `get_mask()`.
///
/// The `from_record()` static factory dispatches to the appropriate subclass
/// based on the `"type"` field of the serialized `Record`.
///
/// Concrete subclasses provided in this header:
/// - `LcBox` — rectangular pixel region (no mask needed).
/// - `LcPixelSet` — arbitrary mask over a bounding box.
/// - `LcEllipsoid` — ellipsoidal mask in pixel coordinates.
/// - `LcPolygon` — 2D polygonal mask in pixel coordinates.
/// - `LcMask` — full-lattice in-memory boolean mask.
/// - `LcPagedMask` — persistent mask (stub).
/// - `LcUnion`, `LcIntersection`, `LcDifference`, `LcComplement`, `LcExtension`
///   — set-algebra compound regions.
///
///
/// @par Motivation
/// Separating the pixel-domain representation (`LcRegion`) from the world-
/// coordinate representation (`WcRegion`) allows image algorithms to work
/// entirely with integer pixel ranges and boolean masks once the coordinate
/// transformation has been applied, without re-evaluating the WCS for each
/// pixel.
///
class LcRegion {
  public:
    virtual ~LcRegion() = default;

    /// Type name for serialization dispatch (e.g., "LcBox").
    [[nodiscard]] virtual std::string type() const = 0;
    /// Deep clone.
    [[nodiscard]] virtual std::unique_ptr<LcRegion> clone() const = 0;
    /// Serialize to Record.
    [[nodiscard]] virtual Record to_record() const = 0;

    /// Dimensionality.
    [[nodiscard]] std::size_t ndim() const {
        return lattice_shape_.ndim();
    }
    /// Shape of the lattice this region lives in.
    [[nodiscard]] const IPosition& lattice_shape() const {
        return lattice_shape_;
    }
    /// Bounding box within the lattice.
    [[nodiscard]] const Slicer& bounding_box() const {
        return bounding_box_;
    }
    /// Shape of the bounding box.
    [[nodiscard]] IPosition shape() const {
        return bounding_box_.length();
    }

    /// Whether this region has a non-trivial mask within its bounding box.
    /// If false, all pixels in the bounding box are included.
    [[nodiscard]] virtual bool has_mask() const {
        return false;
    }
    /// Boolean mask over the bounding box (true = included).
    /// Default: all-true mask matching bounding box shape.
    [[nodiscard]] virtual LatticeArray<bool> get_mask() const;

    /// Optional comment/description.
    [[nodiscard]] const std::string& comment() const {
        return comment_;
    }
    void set_comment(std::string c) {
        comment_ = std::move(c);
    }

    /// Restore an LcRegion from a serialized Record (type dispatch).
    [[nodiscard]] static std::unique_ptr<LcRegion> from_record(const Record& rec,
                                                               const IPosition& lattice_shape);

  protected:
    LcRegion() : lattice_shape_(), bounding_box_(IPosition{0}, IPosition{0}) {}
    LcRegion(IPosition lattice_shape, Slicer bounding_box)
        : lattice_shape_(std::move(lattice_shape)), bounding_box_(std::move(bounding_box)) {}

    void set_bounding_box(Slicer bb) {
        bounding_box_ = std::move(bb);
    }

  private:
    IPosition lattice_shape_;
    Slicer bounding_box_;
    std::string comment_;
};

// ── LcBox ────────────────────────────────────────────────────────────

///
/// Rectangular region in pixel coordinates.
///
///
///
///
///
/// `LcBox` is the simplest `LcRegion`: an axis-aligned rectangle defined by
/// `blc` (bottom-left corner) and `trc` (top-right corner) in 0-based pixel
/// coordinates.  Because the region is a full rectangle, `has_mask()` returns
/// `false` and `get_mask()` produces an all-true mask.
///
/// Constructors accept either explicit corner pairs or a `Slicer`.
///
class LcBox : public LcRegion {
  public:
    /// Construct from explicit corners.
    LcBox(IPosition blc, IPosition trc, IPosition lattice_shape);
    /// Construct from a Slicer.
    LcBox(Slicer slicer, IPosition lattice_shape);

    [[nodiscard]] std::string type() const override {
        return "LcBox";
    }
    [[nodiscard]] std::unique_ptr<LcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] static std::unique_ptr<LcBox> from_record(const Record& rec,
                                                            const IPosition& lattice_shape);

    /// Bottom-left corner.
    [[nodiscard]] const IPosition& blc() const {
        return blc_;
    }
    /// Top-right corner.
    [[nodiscard]] const IPosition& trc() const {
        return trc_;
    }

  private:
    IPosition blc_;
    IPosition trc_;
};

// ── LcPixelSet ───────────────────────────────────────────────────────

///
/// Region defined by an explicit boolean mask over a bounding box.
///
///
///
///
///
/// `LcPixelSet` allows any arbitrary shape to be expressed as a boolean mask
/// over a rectangular sub-region of the lattice.  The `blc` of the sub-region
/// is specified at construction; the mask dimensions define the bounding-box
/// size implicitly.
///
class LcPixelSet : public LcRegion {
  public:
    /// Construct from a mask and its origin in the lattice.
    LcPixelSet(LatticeArray<bool> mask, IPosition blc, IPosition lattice_shape);

    [[nodiscard]] std::string type() const override {
        return "LcPixelSet";
    }
    [[nodiscard]] std::unique_ptr<LcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] bool has_mask() const override {
        return true;
    }
    [[nodiscard]] LatticeArray<bool> get_mask() const override {
        return mask_;
    }

    [[nodiscard]] static std::unique_ptr<LcPixelSet> from_record(const Record& rec,
                                                                 const IPosition& lattice_shape);

  private:
    LatticeArray<bool> mask_;
};

// ── LcEllipsoid ──────────────────────────────────────────────────────

/// Ellipsoidal region in pixel coordinates.
class LcEllipsoid : public LcRegion {
  public:
    /// Construct from center, semi-axes, and lattice shape.
    LcEllipsoid(std::vector<double> center, std::vector<double> semi_axes, IPosition lattice_shape);

    [[nodiscard]] std::string type() const override {
        return "LcEllipsoid";
    }
    [[nodiscard]] std::unique_ptr<LcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] bool has_mask() const override {
        return true;
    }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;

    [[nodiscard]] static std::unique_ptr<LcEllipsoid> from_record(const Record& rec,
                                                                  const IPosition& lattice_shape);

    [[nodiscard]] const std::vector<double>& center() const {
        return center_;
    }
    [[nodiscard]] const std::vector<double>& semi_axes() const {
        return semi_axes_;
    }

  private:
    void compute_bounding_box();

    std::vector<double> center_;
    std::vector<double> semi_axes_;
};

// ── LcPolygon ────────────────────────────────────────────────────────

/// Polygonal region in 2D pixel coordinates.
class LcPolygon : public LcRegion {
  public:
    /// Construct from vertex x/y arrays and 2D lattice shape.
    LcPolygon(std::vector<double> x, std::vector<double> y, IPosition lattice_shape);

    [[nodiscard]] std::string type() const override {
        return "LcPolygon";
    }
    [[nodiscard]] std::unique_ptr<LcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] bool has_mask() const override {
        return true;
    }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;

    [[nodiscard]] static std::unique_ptr<LcPolygon> from_record(const Record& rec,
                                                                const IPosition& lattice_shape);

    [[nodiscard]] const std::vector<double>& x() const {
        return x_;
    }
    [[nodiscard]] const std::vector<double>& y() const {
        return y_;
    }

  private:
    void compute_bounding_box_and_mask();

    std::vector<double> x_;
    std::vector<double> y_;
    LatticeArray<bool> mask_cache_;
};

// ── LcMask ───────────────────────────────────────────────────────────

/// Temporary in-memory mask over the full lattice.
class LcMask : public LcRegion {
  public:
    /// Construct from a full-lattice mask.
    LcMask(LatticeArray<bool> mask, IPosition lattice_shape);

    [[nodiscard]] std::string type() const override {
        return "LcMask";
    }
    [[nodiscard]] std::unique_ptr<LcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] bool has_mask() const override {
        return true;
    }
    [[nodiscard]] LatticeArray<bool> get_mask() const override {
        return mask_;
    }

  private:
    LatticeArray<bool> mask_;
};

// ── LcPagedMask ──────────────────────────────────────────────────────

/// Persistent mask stored in a table column. Stub for future implementation.
class LcPagedMask : public LcRegion {
  public:
    LcPagedMask(IPosition lattice_shape, IPosition mask_shape);

    [[nodiscard]] std::string type() const override {
        return "LcPagedMask";
    }
    [[nodiscard]] std::unique_ptr<LcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] bool has_mask() const override {
        return true;
    }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;

  private:
    IPosition mask_shape_;
};

// ── Compound regions ─────────────────────────────────────────────────

/// Union of multiple LC regions.
class LcUnion : public LcRegion {
  public:
    LcUnion(std::vector<std::unique_ptr<LcRegion>> regions, IPosition lattice_shape);

    [[nodiscard]] std::string type() const override {
        return "LcUnion";
    }
    [[nodiscard]] std::unique_ptr<LcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] bool has_mask() const override {
        return true;
    }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;

    [[nodiscard]] std::size_t n_regions() const {
        return regions_.size();
    }
    [[nodiscard]] const LcRegion& region(std::size_t i) const {
        return *regions_.at(i);
    }

  private:
    std::vector<std::unique_ptr<LcRegion>> regions_;
};

/// Intersection of multiple LC regions.
class LcIntersection : public LcRegion {
  public:
    LcIntersection(std::vector<std::unique_ptr<LcRegion>> regions, IPosition lattice_shape);

    [[nodiscard]] std::string type() const override {
        return "LcIntersection";
    }
    [[nodiscard]] std::unique_ptr<LcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] bool has_mask() const override {
        return true;
    }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;

    [[nodiscard]] std::size_t n_regions() const {
        return regions_.size();
    }
    [[nodiscard]] const LcRegion& region(std::size_t i) const {
        return *regions_.at(i);
    }

  private:
    std::vector<std::unique_ptr<LcRegion>> regions_;
};

/// Difference of two LC regions (first minus second).
class LcDifference : public LcRegion {
  public:
    LcDifference(std::unique_ptr<LcRegion> region1, std::unique_ptr<LcRegion> region2,
                 IPosition lattice_shape);

    [[nodiscard]] std::string type() const override {
        return "LcDifference";
    }
    [[nodiscard]] std::unique_ptr<LcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] bool has_mask() const override {
        return true;
    }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;

  private:
    std::unique_ptr<LcRegion> region1_;
    std::unique_ptr<LcRegion> region2_;
};

/// Complement of an LC region.
class LcComplement : public LcRegion {
  public:
    LcComplement(std::unique_ptr<LcRegion> region, IPosition lattice_shape);

    [[nodiscard]] std::string type() const override {
        return "LcComplement";
    }
    [[nodiscard]] std::unique_ptr<LcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] bool has_mask() const override {
        return true;
    }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;

  private:
    std::unique_ptr<LcRegion> region_;
};

/// Extension of a region along additional axes.
class LcExtension : public LcRegion {
  public:
    /// Extend `region` along axes specified by `extend_axes` with given lengths.
    LcExtension(std::unique_ptr<LcRegion> region, std::vector<std::size_t> extend_axes,
                IPosition extend_lengths, IPosition lattice_shape);

    [[nodiscard]] std::string type() const override {
        return "LcExtension";
    }
    [[nodiscard]] std::unique_ptr<LcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] bool has_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask() const override;

  private:
    std::unique_ptr<LcRegion> region_;
    std::vector<std::size_t> extend_axes_;
    IPosition extend_lengths_;
};

// ── LatticeRegion ────────────────────────────────────────────────────

///
/// Holder wrapping an LcRegion with optional stride/slicer support.
///
///
///
///
///
/// `LatticeRegion` is a thin wrapper around a heap-allocated `LcRegion` that
/// optionally associates a `Slicer` for strided sub-lattice access.  It is
/// the type stored in image keywords for named pixel-domain regions.
///
/// `has_slicer()` is `true` when the region was constructed from a `Slicer`.
/// Both the enclosed `LcRegion` and the `Slicer` are serializable to and from
/// a `Record`.
///
class LatticeRegion {
  public:
    LatticeRegion() = default;
    explicit LatticeRegion(std::unique_ptr<LcRegion> region);
    LatticeRegion(std::unique_ptr<LcRegion> region, Slicer slicer);

    [[nodiscard]] bool has_region() const {
        return region_ != nullptr;
    }
    [[nodiscard]] const LcRegion& region() const {
        return *region_;
    }
    [[nodiscard]] const Slicer& slicer() const {
        return slicer_;
    }
    [[nodiscard]] bool has_slicer() const {
        return has_slicer_;
    }

    /// Serialize to Record.
    [[nodiscard]] Record to_record() const;
    /// Restore from Record.
    [[nodiscard]] static LatticeRegion from_record(const Record& rec,
                                                   const IPosition& lattice_shape);

  private:
    std::unique_ptr<LcRegion> region_;
    Slicer slicer_{IPosition{0}, IPosition{0}};
    bool has_slicer_ = false;
};

/// @}
} // namespace casacore_mini
