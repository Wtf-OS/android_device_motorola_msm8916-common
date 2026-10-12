#pragma once
#include "qmi-backend.h"

// shortcut: only one slot without SIM applications; add application decoding with a real SIM.
struct UimCardStatus {
    bool valid=false;
    int state=2;
    int pin=0;
    static UimCardStatus parse(const std::vector<uint8_t>& value) {
        UimCardStatus card;
        if (value.size()!=15 || value[8]!=1 || value[9]>2 || value[10]>5 || value[14]!=0)
            return card;
        for (size_t off=0;off<8;off+=2) {
            if (QmiReply::u16(value.data()+off)!=0xffff) return card;
        }
        card.valid=true;
        card.state=value[9];
        card.pin=value[10];
        return card;
    }
};
