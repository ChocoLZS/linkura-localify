#ifndef LINKURA_LOCALIFY_MASTERLOCAL_H
#define LINKURA_LOCALIFY_MASTERLOCAL_H

#include <string>

namespace LinkuraLocal::MasterLocal {
    void LoadData();

    void LocalizeMasterItem(void* item, const std::string& tableName);
}

#endif //LINKURA_LOCALIFY_MASTERLOCAL_H
