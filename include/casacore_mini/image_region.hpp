// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/coordinate_system.hpp"
#include "casacore_mini/lattice_region.hpp"
#include "casacore_mini/record.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief World-coordinate (WC) region hierarchy and ImageRegion container.
///
/// A WcRegion describes a region in world coordinates. It is converted to an
/// LcRegion (pixel coordinates) via `to_lc_region()` given a CoordinateSystem
/// and lattice shape.
/// @ingroup images
/// @addtogroup images
/// @{

// ── WcRegion (abstract base) ─────────────────────────────────────────

///
/// Abstract base class for world-coordinate regions.
///
///
///
///
///
/// A `WcRegion` describes a spatial, spectral, or combined region expressed
/// in world (physical) coordinates.  To apply a `WcRegion` to actual image
/// data it must be converted to an `LcRegion` via `to_lc_region()`, which
/// takes a `CoordinateSystem` and the lattice shape to perform the
/// world-to-pixel coordinate transformation.
///
/// The `type()` string uniquely identifies the concrete subclass and is used
/// by `WcRegion::from_record()` for deserialization dispatch.  All subclasses
/// must implement `clone()`, `to_record()`, `ndim()`, and `to_lc_region()`.
///
/// Concrete subclasses provided in this header:
/// - `WcBox` — rectangular world-coordinate box.
/// - `WcEllipsoid` — ellipsoidal world-coordinate region.
/// - `WcPolygon` — 2D polygonal world-coordinate region.
/// - `WcUnion`, `WcIntersection`, `WcDifference`, `WcComplement` — set algebra.
///
class WcRegion {
  public:
    virtual ~WcRegion() = default;

    /// Type name for serialization dispatch.
    [[nodiscard]] virtual std::string type() const = 0;
    /// Deep clone.
    [[nodiscard]] virtual std::unique_ptr<WcRegion> clone() const = 0;
    /// Serialize to Record.
    [[nodiscard]] virtual Record to_record() const = 0;
    /// Number of axes this region constrains.
    [[nodiscard]] virtual std::size_t ndim() const = 0;

    /// Convert to a pixel-domain LcRegion given a coordinate system and shape.
    [[nodiscard]] virtual std::unique_ptr<LcRegion>
    to_lc_region(const CoordinateSystem& cs, const IPosition& lattice_shape) const = 0;

    /// Optional comment.
    [[nodiscard]] const std::string& comment() const {
        return comment_;
    }
    void set_comment(std::string c) {
        comment_ = std::move(c);
    }

    /// Restore a WcRegion from a serialized Record (type dispatch).
    [[nodiscard]] static std::unique_ptr<WcRegion> from_record(const Record& rec);

  private:
    std::string comment_;
};

// ── WcBox ────────────────────────────────────────────────────────────

///
/// Rectangular region in world coordinates.
///
///
///
///
///
/// `WcBox` defines an N-dimensional axis-aligned box using world-coordinate
/// corners `blc` (bottom-left) and `trc` (top-right).  The axis names and
/// units are stored to allow unambiguous mapping to pixel axes when
/// `to_lc_region()` is called.
///
class WcBox : public WcRegion {
  public:
    /// Construct from world-coordinate corners.
    /// @param blc  Bottom-left corner in world coordinates.
    /// @param trc  Top-right corner in world coordinates.
    /// @param axis_names  Names of axes (e.g., "Right Ascension", "Frequency").
    /// @param axis_units  Units for each axis (e.g., "rad", "Hz").
    WcBox(std::vector<double> blc, std::vector<double> trc, std::vector<std::string> axis_names,
          std::vector<std::string> axis_units);

    [[nodiscard]] std::string type() const override {
        return "WcBox";
    }
    [[nodiscard]] std::unique_ptr<WcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] std::size_t ndim() const override {
        return blc_.size();
    }

    [[nodiscard]] std::unique_ptr<LcRegion>
    to_lc_region(const CoordinateSystem& cs, const IPosition& lattice_shape) const override;

    [[nodiscard]] static std::unique_ptr<WcBox> from_record(const Record& rec);

    [[nodiscard]] const std::vector<double>& blc() const {
        return blc_;
    }
    [[nodiscard]] const std::vector<double>& trc() const {
        return trc_;
    }

  private:
    std::vector<double> blc_;
    std::vector<double> trc_;
    std::vector<std::string> axis_names_;
    std::vector<std::string> axis_units_;
};

// ── WcEllipsoid ──────────────────────────────────────────────────────

///
/// Ellipsoidal region in world coordinates.
///
///
///
///
///
/// `WcEllipsoid` is defined by a center point and a vector of semi-axis radii
/// in world coordinates.  The number of axes matches `center.size()`.
///
class WcEllipsoid : public WcRegion {
  public:
    WcEllipsoid(std::vector<double> center, std::vector<double> radii,
                std::vector<std::string> axis_names, std::vector<std::string> axis_units);

    [[nodiscard]] std::string type() const override {
        return "WcEllipsoid";
    }
    [[nodiscard]] std::unique_ptr<WcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] std::size_t ndim() const override {
        return center_.size();
    }

    [[nodiscard]] std::unique_ptr<LcRegion>
    to_lc_region(const CoordinateSystem& cs, const IPosition& lattice_shape) const override;

    [[nodiscard]] static std::unique_ptr<WcEllipsoid> from_record(const Record& rec);

    [[nodiscard]] const std::vector<double>& center() const {
        return center_;
    }
    [[nodiscard]] const std::vector<double>& radii() const {
        return radii_;
    }

  private:
    std::vector<double> center_;
    std::vector<double> radii_;
    std::vector<std::string> axis_names_;
    std::vector<std::string> axis_units_;
};

// ── WcPolygon ────────────────────────────────────────────────────────

///
/// 2D polygonal region in world coordinates.
///
///
///
///
///
/// `WcPolygon` is a 2D region defined by arrays of world-coordinate vertex
/// positions along two named axes.  The polygon is always two-dimensional
/// (`ndim()` returns 2).
///
class WcPolygon : public WcRegion {
  public:
    WcPolygon(std::vector<double> x, std::vector<double> y, std::string x_axis_name,
              std::string y_axis_name, std::string x_axis_unit, std::string y_axis_unit);

    [[nodiscard]] std::string type() const override {
        return "WcPolygon";
    }
    [[nodiscard]] std::unique_ptr<WcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] std::size_t ndim() const override {
        return 2;
    }

    [[nodiscard]] std::unique_ptr<LcRegion>
    to_lc_region(const CoordinateSystem& cs, const IPosition& lattice_shape) const override;

    [[nodiscard]] static std::unique_ptr<WcPolygon> from_record(const Record& rec);

    [[nodiscard]] const std::vector<double>& x() const {
        return x_;
    }
    [[nodiscard]] const std::vector<double>& y() const {
        return y_;
    }

  private:
    std::vector<double> x_;
    std::vector<double> y_;
    std::string x_axis_name_;
    std::string y_axis_name_;
    std::string x_axis_unit_;
    std::string y_axis_unit_;
};

// ── WcUnion ──────────────────────────────────────────────────────────

/// Union of multiple WC regions.
class WcUnion : public WcRegion {
  public:
    WcUnion(std::vector<std::unique_ptr<WcRegion>> regions);

    [[nodiscard]] std::string type() const override {
        return "WcUnion";
    }
    [[nodiscard]] std::unique_ptr<WcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] std::size_t ndim() const override;

    [[nodiscard]] std::unique_ptr<LcRegion>
    to_lc_region(const CoordinateSystem& cs, const IPosition& lattice_shape) const override;

    [[nodiscard]] std::size_t n_regions() const {
        return regions_.size();
    }

  private:
    std::vector<std::unique_ptr<WcRegion>> regions_;
};

// ── WcIntersection ───────────────────────────────────────────────────

/// Intersection of multiple WC regions.
class WcIntersection : public WcRegion {
  public:
    WcIntersection(std::vector<std::unique_ptr<WcRegion>> regions);

    [[nodiscard]] std::string type() const override {
        return "WcIntersection";
    }
    [[nodiscard]] std::unique_ptr<WcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] std::size_t ndim() const override;

    [[nodiscard]] std::unique_ptr<LcRegion>
    to_lc_region(const CoordinateSystem& cs, const IPosition& lattice_shape) const override;

    [[nodiscard]] std::size_t n_regions() const {
        return regions_.size();
    }

  private:
    std::vector<std::unique_ptr<WcRegion>> regions_;
};

// ── WcDifference ─────────────────────────────────────────────────────

/// Difference of two WC regions (first minus second).
class WcDifference : public WcRegion {
  public:
    WcDifference(std::unique_ptr<WcRegion> region1, std::unique_ptr<WcRegion> region2);

    [[nodiscard]] std::string type() const override {
        return "WcDifference";
    }
    [[nodiscard]] std::unique_ptr<WcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] std::size_t ndim() const override;

    [[nodiscard]] std::unique_ptr<LcRegion>
    to_lc_region(const CoordinateSystem& cs, const IPosition& lattice_shape) const override;

  private:
    std::unique_ptr<WcRegion> region1_;
    std::unique_ptr<WcRegion> region2_;
};

// ── WcComplement ─────────────────────────────────────────────────────

/// Complement of a WC region.
class WcComplement : public WcRegion {
  public:
    explicit WcComplement(std::unique_ptr<WcRegion> region);

    [[nodiscard]] std::string type() const override {
        return "WcComplement";
    }
    [[nodiscard]] std::unique_ptr<WcRegion> clone() const override;
    [[nodiscard]] Record to_record() const override;
    [[nodiscard]] std::size_t ndim() const override;

    [[nodiscard]] std::unique_ptr<LcRegion>
    to_lc_region(const CoordinateSystem& cs, const IPosition& lattice_shape) const override;

  private:
    std::unique_ptr<WcRegion> region_;
};

// ── ImageRegion ──────────────────────────────────────────────────────

/// Discriminator for which kind of region is stored.
enum class RegionKind : std::uint8_t {
    lc_region, ///< Pixel-coordinate region.
    wc_region, ///< World-coordinate region.
    slicer,    ///< Simple slicer (box without mask).
};

///
/// Unified container holding either an LcRegion, WcRegion, or Slicer.
///
///
///
///
///
/// `ImageRegion` is a type-safe discriminated union over the three region
/// representations used by casacore images.  It is the type stored in image
/// keywords for named regions via `RegionHandler`.
///
/// `to_lc_region()` converts any variant to an `LcRegion` in pixel
/// coordinates:
/// - If already an `LcRegion`, returns a clone.
/// - If a `WcRegion`, delegates to `WcRegion::to_lc_region()`.
/// - If a `Slicer`, creates an `LcBox`.
///
/// Serialization round-trips through `to_record()` and `from_record()`.
///
class ImageRegion {
  public:
    ImageRegion() = default;
    explicit ImageRegion(std::unique_ptr<LcRegion> region);
    explicit ImageRegion(std::unique_ptr<WcRegion> region);
    explicit ImageRegion(Slicer slicer);

    [[nodiscard]] RegionKind kind() const {
        return kind_;
    }

    [[nodiscard]] bool has_lc_region() const {
        return lc_region_ != nullptr;
    }
    [[nodiscard]] bool has_wc_region() const {
        return wc_region_ != nullptr;
    }
    [[nodiscard]] bool has_slicer() const {
        return kind_ == RegionKind::slicer;
    }

    [[nodiscard]] const LcRegion& as_lc_region() const {
        return *lc_region_;
    }
    [[nodiscard]] const WcRegion& as_wc_region() const {
        return *wc_region_;
    }
    [[nodiscard]] const Slicer& as_slicer() const {
        return slicer_;
    }

    /// Convert to an LcRegion given a coordinate system and shape.
    /// If already an LcRegion, returns a clone. If a WcRegion, converts.
    /// If a Slicer, creates an LcBox.
    [[nodiscard]] std::unique_ptr<LcRegion> to_lc_region(const CoordinateSystem& cs,
                                                         const IPosition& lattice_shape) const;

    /// Serialize to Record.
    [[nodiscard]] Record to_record() const;
    /// Restore from Record.
    [[nodiscard]] static ImageRegion from_record(const Record& rec, const IPosition& lattice_shape);

  private:
    RegionKind kind_ = RegionKind::lc_region;
    std::unique_ptr<LcRegion> lc_region_;
    std::unique_ptr<WcRegion> wc_region_;
    Slicer slicer_{IPosition{0}, IPosition{0}};
};

// ── RegionHandler ────────────────────────────────────────────────────

///
/// Manages named regions stored as keywords in an image.
///
///
///
///
///
/// `RegionHandler` maintains a collection of `ImageRegion` objects keyed by
/// name, and an optional default mask name.  It is embedded in image objects
/// to provide the casacore `ImageRegion` keyword API.
///
/// Regions can be defined, retrieved, removed, and renamed.  The entire
/// collection serializes to and from a `Record` for storage in image keyword
/// tables.
///
class RegionHandler {
  public:
    /// Define a named region in this handler's collection.
    void define_region(std::string name, ImageRegion region);
    /// Check if a named region exists.
    [[nodiscard]] bool has_region(const std::string& name) const;
    /// Get a named region.
    [[nodiscard]] const ImageRegion& get_region(const std::string& name) const;
    /// Remove a named region.
    void remove_region(const std::string& name);
    /// Rename a region.
    void rename_region(const std::string& old_name, const std::string& new_name);
    /// Get all region names.
    [[nodiscard]] std::vector<std::string> region_names() const;
    /// Number of regions.
    [[nodiscard]] std::size_t n_regions() const {
        return regions_.size();
    }

    /// Get the default mask name (empty if none).
    [[nodiscard]] const std::string& default_mask() const {
        return default_mask_;
    }
    /// Set the default mask name.
    void set_default_mask(std::string name) {
        default_mask_ = std::move(name);
    }

    /// Serialize all regions to a Record.
    [[nodiscard]] Record to_record() const;
    /// Restore from Record.
    void from_record(const Record& rec, const IPosition& lattice_shape);

  private:
    std::unordered_map<std::string, ImageRegion> regions_;
    std::string default_mask_;
};

// ── RegionManager ────────────────────────────────────────────────────

///
/// Factory/utility class for creating common region types.
///
///
///
///
///
/// `RegionManager` provides static factory methods that construct the common
/// world-coordinate region types.  All methods return heap-allocated unique
/// pointers to the appropriate `WcRegion` subclass.
///
/// This class has no state; all methods are static.
///
class RegionManager {
  public:
    /// Create a WcBox from world-coordinate corners.
    [[nodiscard]] static std::unique_ptr<WcBox> wc_box(std::vector<double> blc,
                                                       std::vector<double> trc,
                                                       std::vector<std::string> axis_names,
                                                       std::vector<std::string> axis_units);

    /// Create a WcEllipsoid.
    [[nodiscard]] static std::unique_ptr<WcEllipsoid>
    wc_ellipsoid(std::vector<double> center, std::vector<double> radii,
                 std::vector<std::string> axis_names, std::vector<std::string> axis_units);

    /// Create a WcPolygon.
    [[nodiscard]] static std::unique_ptr<WcPolygon>
    wc_polygon(std::vector<double> x, std::vector<double> y, std::string x_axis_name,
               std::string y_axis_name, std::string x_axis_unit, std::string y_axis_unit);

    /// Combine regions into a union.
    [[nodiscard]] static std::unique_ptr<WcUnion>
    make_union(std::vector<std::unique_ptr<WcRegion>> regions);

    /// Combine regions into an intersection.
    [[nodiscard]] static std::unique_ptr<WcIntersection>
    make_intersection(std::vector<std::unique_ptr<WcRegion>> regions);

    /// Create a difference (first minus second).
    [[nodiscard]] static std::unique_ptr<WcDifference>
    make_difference(std::unique_ptr<WcRegion> r1, std::unique_ptr<WcRegion> r2);

    /// Create a complement.
    [[nodiscard]] static std::unique_ptr<WcComplement>
    make_complement(std::unique_ptr<WcRegion> region);
};

/// @}
} // namespace casacore_mini
