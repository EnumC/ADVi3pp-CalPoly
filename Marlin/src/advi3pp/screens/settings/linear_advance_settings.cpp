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
#include "linear_advance_settings.h"
#include "../../core/dgus.h"

namespace ADVi3pp {

LinearAdvanceSettings linear_advance_settings;

//! Prepare the page before being displayed and return the right Page value
//! @return The index of the page to display
Page LinearAdvanceSettings::do_prepare_page()
{
    WriteRamRequest{Variable::Value0}.write_word(ExtUI::getLinearAdvance_mm_mm_s(ExtUI::E0) * 100);
    return Page::LinearAdvanceSettings;
}

//! Handles the Save (Continue) command
void LinearAdvanceSettings::do_save_command()
{
    ReadRam response{Variable::Value0};
    if(!response.send_receive(1))
    {
        Log::error() << F("Receiving Frame (Linear Advance Settings)") << Log::endl();
        return;
    }

    uint16_t k = response.read_word();
    ExtUI::setLinearAdvance_mm_mm_s(k / 100.0, ExtUI::E0);

    Parent::do_save_command();
}

}
