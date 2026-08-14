#include "DsspOption.h"

void DsspOption::load(const QJsonObject &object) {
    load_enabled(object);
    load_host(object);
    load_port(object);
}

void DsspOption::save(QJsonObject &object) {
    object = {
        serialize_enabled(),
        serialize_host(),
        serialize_port(),
    };
}
