/**
 * ADVi3++ Firmware For Wanhao Duplicator i3 Plus (based on Marlin 2)
 *
 * Copyright (C) 2017-2021 Sebastien Andrivet [https://github.com/andrivet/]
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "../../parameters.h"
#include "../../core/core.h"
#include "../../screens/core/wait.h"
#include "xtwist.h"

namespace ADVi3pp {

namespace {

    const float FEEDRATE_XY = MMM_TO_MMS(HOMING_FEEDRATE_XY);
    const float FEEDRATE_Z = MMM_TO_MMS(HOMING_FEEDRATE_Z);

}

XTwist xtwist;

#ifdef ADVi3PP_PROBE

const double SENSOR_Z_HEIGHT_MULTIPLIERS[] = {0.02, 0.10, 1.0};
const int MARGIN = 10;

void twist(xyze_pos_t &pos)
{
    auto z = xtwist.compute_z(pos.x);
    Log::log() << F("X-Twist") << pos.x << F("->") << z << Log::endl();
    pos.z += z;
}

void untwist(xyze_pos_t &pos)
{
    auto z = xtwist.compute_z(pos.x);
    Log::log() << F("X-UnTwist") << pos.x << F("->") << z << Log::endl();
    pos.z -= z;
}

float twist_offset_x0() { return xtwist.get_offset(XTwist::Point::L); }
float twist_offset_x2() { return xtwist.get_offset(XTwist::Point::R); }

//! Store current data in permanent memory (EEPROM)
//! @param eeprom EEPROM writer
void XTwist::do_write(EepromWrite& eeprom) const
{
    eeprom.write(offset(Point::L));
    eeprom.write(offset(Point::R));
}

//! Validate data from permanent memory (EEPROM).
//! @param eeprom EEPROM reader
bool XTwist::do_validate(EepromRead &eeprom)
{
    float dummy{};
    eeprom.read(dummy);
    eeprom.read(dummy);
    return true;
}

//! Restore data from permanent memory (EEPROM).
//! @param eeprom EEPROM reader
void XTwist::do_read(EepromRead& eeprom)
{
    eeprom.read(offset(Point::L));
    eeprom.read(offset(Point::R));
    offset(Point::M) = 0;
    compute_factors();
}

//! Reset settings
void XTwist::do_reset()
{
    offsets_.fill(0);
    a_ = b_ = c_ = 0.0;
}

//! Return the amount of data (in bytes) necessary to save settings in permanent memory (EEPROM).
//! @return Number of bytes
uint16_t XTwist::do_size_of() const
{
    return 2 * sizeof(long);
}

//! Handle Sensor Z Height command
//! @param key_value    The sub-action to handle
//! @return             True if the action was handled
bool XTwist::do_dispatch(KeyValue key_value)
{
    if(Parent::do_dispatch(key_value))
        return true;

    switch(key_value)
    {
        case KeyValue::Multiplier1:     multiplier1_command(); break;
        case KeyValue::Multiplier2:     multiplier2_command(); break;
        case KeyValue::Multiplier3:     multiplier3_command(); break;
        case KeyValue::Point_L:         point_L_command(); break;
        case KeyValue::Point_M:         point_M_command(); break;
        case KeyValue::Point_R:         point_R_command(); break;
        default:                        return false;
    }

    return true;
}

//! Prepare the page before being displayed and return the right Page value
//! @return The index of the page to display
Page XTwist::do_prepare_page()
{
    if(!core.ensure_not_printing())
        return Page::None;

    if(!ExtUI::getLevelingActive())
    {
        wait.wait_back(F("Please do an automated bed leveling."));
        return Page::None;
    }

    pages.save_forward_page();

    old_offsets_ = offsets_;
    a_ = b_ = c_ = 0.0;

    wait.wait(F("Homing..."));
    core.inject_commands(F("G28 F6000"));  // homing
    background_task.set(Callback{this, &XTwist::post_home_task}, 200);

    return Page::None;
}

//! Check if the printer is homed, and continue the process.
void XTwist::post_home_task()
{
    if(core.is_busy() || !ExtUI::isMachineHomed())
        return;

    background_task.clear();

    send_data();
    status.reset();

    ExtUI::setSoftEndstopState(false);
    point_M_command();

    pages.show(Page::XTwist);
}

//! Execute the Back command
void XTwist::do_back_command()
{
    offsets_ = old_offsets_;
    compute_factors();

    // enable enstops, raise head
    ExtUI::setSoftEndstopState(true);
    ExtUI::setFeedrate_mm_s(FEEDRATE_Z);
    ExtUI::setAxisPosition_mm(4, ExtUI::Z);

    Parent::do_back_command();
}

//! Handles the Save (Continue) command
void XTwist::do_save_command()
{
    compute_factors();

    // enable enstops, raise head
    ExtUI::setSoftEndstopState(true);
    ExtUI::setFeedrate_mm_s(FEEDRATE_Z);
    ExtUI::setAxisPosition_mm(4, ExtUI::Z);

    Parent::do_save_command();
}

void XTwist::reset()
{
    do_reset();
}

void XTwist::compute_factors()
{
    const auto offset = get_offset(Point::M);
    ExtUI::setZOffset_mm(ExtUI::getZOffset_mm() + offset);

    set_offset(Point::L, get_offset(Point::L) - offset);
    set_offset(Point::R, get_offset(Point::R) - offset);
    set_offset(Point::M, 0);

    double x0 = get_x_mm(Point::L);
    double z0 = get_offset(Point::L);
    double x1 = get_x_mm(Point::M);
    double x2 = get_x_mm(Point::R);
    double z2 = get_offset(Point::R);

    // Quadratic equation - Newton's divided differences interpolation polynomial
    a_ = z0;
    b_ = -z0 / (x1 - x0);
    float b2 = z2 / (x2 - x1);
    c_ = (b2 - b_) / (x2 - x0);
}

float XTwist::compute_z(float x) const
{
    double x0 = get_x_mm(Point::L);
    double x1 = get_x_mm(Point::M);
    return a_ + (x - x0) * b_ + (x - x0) * (x - x1) * c_;
}

//! Change the multiplier.
void XTwist::multiplier1_command()
{
    multiplier_ = Multiplier::M1;
    send_data();
}

//! Change the multiplier.
void XTwist::multiplier2_command()
{
    multiplier_ = Multiplier::M2;
    send_data();
}

//! Change the multiplier.
void XTwist::multiplier3_command()
{
    multiplier_ = Multiplier::M3;
    send_data();
}

float XTwist::get_x_mm(Point x) const
{
    return (x == Point::L ? MARGIN : (x == Point::R ? X_BED_SIZE - MARGIN : (X_BED_SIZE / 2.0f)));
}

void XTwist::move_x(Point x)
{
    ExtUI::setFeedrate_mm_s(FEEDRATE_Z);
    ExtUI::setAxisPosition_mm(4, ExtUI::Z);

    ExtUI::setFeedrate_mm_s(FEEDRATE_XY);
    ExtUI::setAxisPosition_mm(get_x_mm(x), ExtUI::X);

    ExtUI::setFeedrate_mm_s(FEEDRATE_XY);
    ExtUI::setAxisPosition_mm(Y_BED_SIZE / 2.0f, ExtUI::Y);

    ExtUI::setFeedrate_mm_s(FEEDRATE_Z);
    ExtUI::setAxisPosition_mm(get_offset(x), ExtUI::Z);

    point_ = x;
}

void XTwist::point_M_command()
{
    move_x(Point::M);
}

void XTwist::point_L_command()
{
    move_x(Point::L);
}

void XTwist::point_R_command()
{
    move_x(Point::R);
}

//! Change the position of the nozzle (-Z).
void XTwist::minus()
{
    adjust_height(-get_multiplier_value());
}

//! Change the position of the nozzle (+Z).
void XTwist::plus()
{
    adjust_height(+get_multiplier_value());
}

//! Get the current multiplier value on the LCD panel.
double XTwist::get_multiplier_value() const
{
    if(multiplier_ < Multiplier::M1 || multiplier_ > Multiplier::M3)
    {
        Log::error() << F("Invalid multiplier value: ") << static_cast<uint16_t >(multiplier_) << Log::endl();
        return SENSOR_Z_HEIGHT_MULTIPLIERS[0];
    }

    return SENSOR_Z_HEIGHT_MULTIPLIERS[static_cast<uint16_t>(multiplier_)];
}

//! Adjust the Z height.
//! @param offset Offset for the adjustment.
void XTwist::adjust_height(double offset_value)
{
    ExtUI::setFeedrate_mm_s(FEEDRATE_Z);
    ExtUI::setAxisPosition_mm(ExtUI::getAxisPosition_mm(ExtUI::Z) + offset_value, ExtUI::Z);
    set_offset(point_, ExtUI::getAxisPosition_mm(ExtUI::Z));
}

//! Send the current data (i.e. multiplier) to the LCD panel.
void XTwist::send_data() const
{
    WriteRamRequest{Variable::Value0}.write_word(static_cast<uint16_t>(multiplier_));
}

#else

//! Prepare the page before being displayed and return the right Page value
//! @return The index of the page to display
Page XTwist::do_prepare_page()
{
    return Page::NoSensor;
}

#endif

}
