#include "LinkSetupFrame.h"

namespace modemm17
{

const LinkSetupFrame::encoded_call_t LinkSetupFrame::BROADCAST_ADDRESS = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
const LinkSetupFrame::call_t LinkSetupFrame::BROADCAST_CALL = {'B', 'R', 'O', 'A', 'D', 'C', 'A', 'S', 'T', 0};

}
