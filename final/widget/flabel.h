/***********************************************************************
* flabel.cpp - Widget FLabel                                           *
*                                                                      *
* This file is part of the FINAL CUT widget toolkit                    *
*                                                                      *
* Copyright 2014-2021 Markus Gans                                      *
*                                                                      *
* FINAL CUT is free software; you can redistribute it and/or modify    *
* it under the terms of the GNU Lesser General Public License as       *
* published by the Free Software Foundation; either version 3 of       *
* the License, or (at your option) any later version.                  *
*                                                                      *
* FINAL CUT is distributed in the hope that it will be useful, but     *
* WITHOUT ANY WARRANTY; without even the implied warranty of           *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
* GNU Lesser General Public License for more details.                  *
*                                                                      *
* You should have received a copy of the GNU Lesser General Public     *
* License along with this program.  If not, see                        *
* <http://www.gnu.org/licenses/>.                                      *
***********************************************************************/

/*  Inheritance diagram
 *  ═══════════════════
 *
 * ▕▔▔▔▔▔▔▔▔▔▏ ▕▔▔▔▔▔▔▔▔▔▏
 * ▕ FVTerm  ▏ ▕ FObject ▏
 * ▕▁▁▁▁▁▁▁▁▁▏ ▕▁▁▁▁▁▁▁▁▁▏
 *      ▲           ▲
 *      │           │
 *      └─────┬─────┘
 *            │
 *       ▕▔▔▔▔▔▔▔▔▔▏
 *       ▕ FWidget ▏
 *       ▕▁▁▁▁▁▁▁▁▁▏
 *            ▲
 *            │
 *       ▕▔▔▔▔▔▔▔▔▏
 *       ▕ FLabel ▏
 *       ▕▁▁▁▁▁▁▁▁▏
 */

#ifndef FLABEL_H
#define FLABEL_H

#if !defined (USE_FINAL_H) && !defined (COMPILE_FINAL_CUT)
  #error "Only <final/final.h> can be included directly."
#endif

#include <vector>

#include "final/fwidget.h"
#include "final/fwidgetcolors.h"

namespace finalcut
{

//----------------------------------------------------------------------
// class FLabel
//----------------------------------------------------------------------

class FLabel : public FWidget
{
  public:
    // Using-declaration
    using FWidget::setEnable;

    // Constructor
    explicit FLabel (FWidget* = nullptr);
    explicit FLabel (const FString&, FWidget* = nullptr);

    // Disable copy constructor
    FLabel (const FLabel&) = delete;

    // Disable move constructor
    FLabel (FLabel&&) noexcept = delete;

    // Destructor
    ~FLabel() override;

    // Disable copy assignment operator (=)
    FLabel& operator = (const FLabel&) = delete;

    // Disable move assignment operator (=)
    FLabel& operator = (FLabel&&) noexcept = delete;

    // Overloaded operators
    FLabel& operator = (const FString&);
    template <typename typeT>
    FLabel& operator << (const typeT&);
    FLabel& operator << (UniChar);
    FLabel& operator << (const wchar_t);
    const FLabel& operator >> (FString&) const;

    // Accessors
    FString             getClassName() const override;
    FWidget*            getAccelWidget();
    Align               getAlignment() const noexcept;
    FString&            getText() &;

    // Mutators
    void                setAccelWidget (FWidget* = nullptr);
    void                setAlignment (Align) noexcept;
    bool                setEmphasis (bool = true) noexcept;
    bool                unsetEmphasis() noexcept;
    void                resetColors() override;
    bool                setReverseMode (bool = true) noexcept;
    bool                unsetReverseMode() noexcept;
    bool                setEnable (bool = true) override;
    void                setNumber (uLong);
    void                setNumber (long);
    void                setNumber (float, int = FLT_DIG);
    void                setNumber (double, int = DBL_DIG);
    void                setNumber (lDouble, int = LDBL_DIG);
    void                setText (const FString&);

    // Inquiries
    bool                hasEmphasis() const noexcept;
    bool                hasReverseMode() const noexcept;

    // Methods
    void                hide() override;
    void                clear();

    // Event handlers
    void                onMouseDown (FMouseEvent*) override;
    void                onAccel (FAccelEvent*) override;

    // Callback method
    void                cb_accelWidgetDestroyed();

  private:
    // Constants
    static constexpr auto NOT_SET = static_cast<std::size_t>(-1);

    // Methods
    void                init();
    void                setHotkeyAccelerator();
    std::size_t         getAlignOffset (const std::size_t) const;
    void                draw() override;
    void                drawMultiLine();
    void                drawSingleLine();
    void                printLine (FString&);

    // Data members
    FStringList         multiline_text{};
    FString             text{};
    FWidget*            accel_widget{nullptr};
    Align               alignment{Align::Left};
    std::size_t         align_offset{0};
    std::size_t         hotkeypos{NOT_SET};
    std::size_t         column_width{0};
    FColor              emphasis_color{FColor::Default};
    FColor              ellipsis_color{FColor::Default};
    bool                multiline{false};
    bool                emphasis{false};
    bool                reverse_mode{false};
};

// FLabel inline functions
//----------------------------------------------------------------------
template <typename typeT>
inline FLabel& FLabel::operator << (const typeT& s)
{
  FString str{};
  str << s;
  setText(text + str);
  return *this;
}

//----------------------------------------------------------------------
inline FString FLabel::getClassName() const
{ return "FLabel"; }

//----------------------------------------------------------------------
inline FWidget* FLabel::getAccelWidget ()
{ return accel_widget; }

//----------------------------------------------------------------------
inline Align FLabel::getAlignment() const noexcept
{ return alignment; }

//----------------------------------------------------------------------
inline FString& FLabel::getText() &
{ return text; }

//----------------------------------------------------------------------
inline bool FLabel::setEmphasis (bool enable) noexcept
{ return (emphasis = enable); }

//----------------------------------------------------------------------
inline bool FLabel::unsetEmphasis() noexcept
{ return setEmphasis(false); }

//----------------------------------------------------------------------
inline bool FLabel::setReverseMode (bool enable) noexcept
{ return (reverse_mode = enable); }

//----------------------------------------------------------------------
inline bool FLabel::unsetReverseMode() noexcept
{ return setReverseMode(false); }

//----------------------------------------------------------------------
inline void FLabel::setNumber (uLong num)
{ setText(FString().setNumber(num)); }

//----------------------------------------------------------------------
inline void FLabel::setNumber (long num)
{ setText(FString().setNumber(num)); }

//----------------------------------------------------------------------
inline void FLabel::setNumber (float num, int precision)
{ setText(FString().setNumber(num, precision)); }

//----------------------------------------------------------------------
inline void FLabel::setNumber (double num, int precision)
{ setText(FString().setNumber(num, precision)); }

//----------------------------------------------------------------------
inline void FLabel::setNumber (lDouble num, int precision)
{ setText(FString().setNumber(num, precision)); }

//----------------------------------------------------------------------
inline bool FLabel::hasEmphasis() const noexcept
{ return emphasis; }

//----------------------------------------------------------------------
inline bool FLabel::hasReverseMode() const noexcept
{ return reverse_mode; }

//----------------------------------------------------------------------
inline void FLabel::clear()
{ text.clear(); }

}  // namespace finalcut

#endif  // FLABEL_H
