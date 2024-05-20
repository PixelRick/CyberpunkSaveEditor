#pragma once
#include <filesystem>
#include <vector>

#include <redx/core.hpp>
#include <redx/io/bstream.hpp>

#include <redx/csav/node.h>
#include <redx/csav/serial_tree.h>
#include <redx/csav/version.h>

namespace redx::csav {

struct node_tree {
    using node_type        = node_t;
    using shared_node_type = std::shared_ptr<const node_t>;

    version& ver() {
        return m_ver;
    }

    const version& ver() const {
        return m_ver;
    }

    op_status load(std::filesystem::path path);

    // This one makes a backup!
    op_status save(std::filesystem::path path);

    friend ibstream& operator<<(ibstream& st, node_tree& x) {
        x.serialize_in(st);
        return st;
    }

    friend obstream& operator<<(obstream& st, node_tree& x) {
        x.serialize_out(st);
        return st;
    }

    std::vector<serial_node_desc> original_descs;
    shared_node_type              root;

protected:
    void serialize_in(ibstream& st);
    void serialize_out(obstream& st);

    version m_ver;
};

} // namespace redx::csav

namespace redx {

using csav_tree = csav::node_tree;

} // namespace redx
