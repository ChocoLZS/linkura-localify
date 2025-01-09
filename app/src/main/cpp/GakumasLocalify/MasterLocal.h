#ifndef GAKUMAS_LOCALIFY_MASTERLOCAL_H
#define GAKUMAS_LOCALIFY_MASTERLOCAL_H

#include <string>

namespace GakumasLocal::MasterLocal {
    void LoadData();

    void LocalizeMasterItem(void* item, const std::string& tableName);
}

#endif //GAKUMAS_LOCALIFY_MASTERLOCAL_H
