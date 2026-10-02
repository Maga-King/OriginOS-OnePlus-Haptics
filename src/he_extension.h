// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "vibrator_frontend.h"
namespace mio {
binder_status_t performHe(VibratorFrontend&,const std::string&,transaction_code_t,const AParcel*,AParcel*);
}
