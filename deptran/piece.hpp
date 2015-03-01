#pragma once

#include <string>

namespace rococo {

class Piece {
public:
    static Piece *get_piece(int benchmark);

    virtual void reg_all() = 0;

    virtual ~Piece() {}
};


#define BEGIN_PIE(txn, pie, iod) \
    TxnRegistry::reg(txn, pie, iod, \
        [] (DTxn *dtxn, \
            const RequestHeader &header, \
            const Value *input, \
            i32 input_size, \
            i32 *res, \
            Value *output, \
            i32 *output_size, \
            row_map_t *row_map)
#define END_PIE );

} // namespace rcc
