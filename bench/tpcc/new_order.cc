#include "all.h"
#include "generator.h"

namespace rococo {

static uint32_t TXN_TYPE = TPCC_NEW_ORDER;

void TpccChopper::NewOrderInit(TxnRequest &req) {
  status_[TPCC_NEW_ORDER_0] = WAITING;
  status_[TPCC_NEW_ORDER_1] = WAITING;
  status_[TPCC_NEW_ORDER_2] = WAITING;
  status_[TPCC_NEW_ORDER_3] = WAITING;
  status_[TPCC_NEW_ORDER_4] = WAITING;
  int32_t ol_cnt = req.input_[TPCC_VAR_OL_CNT].get_i32();
  n_pieces_all_ = 5 + 4 * ol_cnt;
  for (int i = 0; i < ol_cnt; i++) {
    status_[TPCC_NEW_ORDER_RI(i)] = WAITING;
    status_[TPCC_NEW_ORDER_RS(i)] = WAITING;
    status_[TPCC_NEW_ORDER_WS(i)] = WAITING;
    status_[TPCC_NEW_ORDER_WOL(i)] = WAITING;
  }

  CheckReady();
}

void TpccChopper::new_order_retry() {
  status_[TPCC_NEW_ORDER_0] = WAITING;
  status_[TPCC_NEW_ORDER_1] = WAITING;
  status_[TPCC_NEW_ORDER_2] = WAITING;
  status_[TPCC_NEW_ORDER_3] = WAITING;
  status_[TPCC_NEW_ORDER_4] = WAITING;
  int32_t ol_cnt = ws_[TPCC_VAR_OL_CNT].get_i32();
  for (size_t i = 0; i < ol_cnt; i++) {
    status_[TPCC_NEW_ORDER_Ith_INDEX_ITEM(i)] = WAITING;
    status_[TPCC_NEW_ORDER_Ith_INDEX_IM_STOCK(i)] = WAITING;
    status_[TPCC_NEW_ORDER_Ith_INDEX_DEFER_STOCK(i)] = WAITING;
    status_[TPCC_NEW_ORDER_Ith_INDEX_ORDER_LINE(i)] = WAITING;
  }
  CheckReady();
}

void TpccPiece::reg_new_order() {
  // Ri & W district
  INPUT_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_0, TPCC_VAR_W_ID, TPCC_VAR_D_ID)
  SHARD_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_0, TPCC_TB_DISTRICT, TPCC_VAR_W_ID)
  BEGIN_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_0, DF_NO) {
    verify(input.size() == 2);
    mdb::MultiBlob mb(2);
    mb[0] = input[TPCC_VAR_D_ID].get_blob();
    mb[1] = input[TPCC_VAR_W_ID].get_blob();
    mdb::Row *r = dtxn->Query(dtxn->GetTable(TPCC_TB_DISTRICT),
                              mb,
                              ROW_DISTRICT);
    Value buf(0);
    // R district
    dtxn->ReadColumn(r,
                     TPCC_COL_DISTRICT_D_TAX,
                     &output[TPCC_VAR_D_TAX],
                     TXN_BYPASS);
    dtxn->ReadColumn(r,
                     TPCC_COL_DISTRICT_D_NEXT_O_ID,
                     &buf,
                     TXN_BYPASS); // read d_next_o_id
    output[TPCC_VAR_O_ID] = buf;
    // read d_next_o_id, increment by 1
    buf.set_i32((i32)(buf.get_i32() + 1));
    dtxn->WriteColumn(r,
                      TPCC_COL_DISTRICT_D_NEXT_O_ID,
                      buf,
                      TXN_INSTANT);
    return;
  } END_PIE

  // R warehouse
  INPUT_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_1, TPCC_VAR_W_ID)
  SHARD_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_1, TPCC_TB_WAREHOUSE, TPCC_VAR_W_ID)
  BEGIN_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_1, DF_NO) {
    verify(input.size() == 1);
    Log::debug("TPCC_NEW_ORDER, piece: %d", TPCC_NEW_ORDER_1);
    mdb::Row *r = dtxn->Query(dtxn->GetTable(TPCC_TB_WAREHOUSE),
                              input[TPCC_VAR_W_ID].get_blob(),
                              ROW_WAREHOUSE);
    // R warehouse
    dtxn->ReadColumn(r, TPCC_COL_WAREHOUSE_W_TAX,
                     &output[TPCC_VAR_W_TAX], TXN_BYPASS); // read w_tax
    return;
  } END_PIE

  // R customer //XXX either i or d is ok
  INPUT_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_2,
            TPCC_VAR_W_ID, TPCC_VAR_D_ID, TPCC_VAR_C_ID)
  SHARD_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_2, TPCC_TB_CUSTOMER, TPCC_VAR_W_ID)
  BEGIN_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_2, DF_NO) {
    verify(input.size() == 3);
    Log::debug("TPCC_NEW_ORDER, piece: %d", TPCC_NEW_ORDER_2);

    mdb::MultiBlob mb(3);
    mb[0] = input[TPCC_VAR_C_ID].get_blob();
    mb[1] = input[TPCC_VAR_D_ID].get_blob();
    mb[2] = input[TPCC_VAR_W_ID].get_blob();
    auto table = dtxn->GetTable(TPCC_TB_CUSTOMER);
    mdb::Row *r = dtxn->Query(table, mb, ROW_CUSTOMER);
    // R customer
    dtxn->ReadColumn(r, TPCC_COL_CUSTOMER_C_LAST,
                     &output[TPCC_VAR_C_LAST], TXN_BYPASS);
    dtxn->ReadColumn(r, TPCC_COL_CUSTOMER_C_CREDIT,
                     &output[TPCC_VAR_C_CREDIT], TXN_BYPASS);
    dtxn->ReadColumn(r, TPCC_COL_CUSTOMER_C_DISCOUNT,
                     &output[TPCC_VAR_C_DISCOUNT], TXN_BYPASS);

    return;
  } END_PIE

  // W order
  INPUT_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_3,
            TPCC_VAR_W_ID, TPCC_VAR_D_ID, TPCC_VAR_O_ID, TPCC_VAR_C_ID,
            TPCC_VAR_O_CARRIER_ID, TPCC_VAR_OL_CNT, TPCC_VAR_O_ALL_LOCAL)
  SHARD_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_3, TPCC_TB_ORDER, TPCC_VAR_W_ID)
  BEGIN_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_3, DF_REAL) {
    verify(input.size() == 7);
    Log::debug("TPCC_NEW_ORDER, piece: %d", TPCC_NEW_ORDER_3);
    i32 oi = 0;
    mdb::Table *tbl = dtxn->GetTable(TPCC_TB_ORDER);

    mdb::MultiBlob mb(3);
    mb[0] = input[TPCC_VAR_D_ID].get_blob();
    mb[1] = input[TPCC_VAR_W_ID].get_blob();
    mb[2] = input[TPCC_VAR_C_ID].get_blob();

    mdb::Row *r = dtxn->Query(dtxn->GetTable(TPCC_TB_ORDER_C_ID_SECONDARY),
                              mb,
                              ROW_ORDER_SEC);
    verify(r);
    verify(r->schema_);

    // W order
    std::vector<Value> row_data= {
        input[TPCC_VAR_D_ID],
        input[TPCC_VAR_W_ID],
        input[TPCC_VAR_O_ID],
        input[TPCC_VAR_C_ID],
        Value(std::to_string(time(NULL))),  // o_entry_d
        input[TPCC_VAR_O_CARRIER_ID],
        input[TPCC_VAR_OL_CNT],
        input[TPCC_VAR_O_ALL_LOCAL]
    };
    CREATE_ROW(tbl->schema(), row_data);
//    verify(r->schema_);
//    RCC_KISS(r, 0, false);
//    RCC_KISS(r, 1, false);
//    RCC_KISS(r, 2, false);
//    RCC_KISS(r, 5, false);
//    RCC_SAVE_ROW(r, TPCC_NEW_ORDER_3);
//    RCC_PHASE1_RET;
//    RCC_LOAD_ROW(r, TPCC_NEW_ORDER_3);

    verify(r->schema_);
    dtxn->InsertRow(tbl, r);

    // write TPCC_TB_ORDER_C_ID_SECONDARY
    //mdb::MultiBlob mb(3);
    //mb[0] = input[1].get_blob();
    //mb[1] = input[2].get_blob();
    //mb[2] = input[3].get_blob();
    r = dtxn->Query(dtxn->GetTable(TPCC_TB_ORDER_C_ID_SECONDARY),
                    mb,
                    ROW_ORDER_SEC);
    dtxn->WriteColumn(r, 3, input[TPCC_VAR_W_ID]);
    return;
  } END_PIE

  // W new_order
  INPUT_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_4,
            TPCC_VAR_W_ID, TPCC_VAR_D_ID, TPCC_VAR_O_ID)
  SHARD_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_4, TPCC_TB_NEW_ORDER, TPCC_VAR_W_ID)
  BEGIN_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_4, DF_REAL) {
    verify(input.size() == 3);
    Log_debug("TPCC_NEW_ORDER, piece: %d", TPCC_NEW_ORDER_4);

    mdb::Table *tbl = dtxn->GetTable(TPCC_TB_NEW_ORDER);
    mdb::Row *r = NULL;

    // W new_order
    std::vector<Value> row_data({
            input[TPCC_VAR_D_ID],   // o_d_id
            input[TPCC_VAR_W_ID],   // o_w_id
            input[TPCC_VAR_O_ID],   // o_id
    });

    CREATE_ROW(tbl->schema(), row_data);

//    RCC_KISS(r, 0, false);
//    RCC_KISS(r, 1, false);
//    RCC_KISS(r, 2, false);

    dtxn->InsertRow(tbl, r);
    *res = SUCCESS;
    return;
  } END_PIE

  for (int i = (0); i < (1000); i++) {
    // 1000 is a magical number?
    SHARD_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_RI(i),
              TPCC_TB_ITEM, TPCC_VAR_I_ID(i))
    INPUT_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_RI(i), TPCC_VAR_I_ID(i))
  }

  BEGIN_LOOP_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_RI(0), 1000, DF_NO)
    verify(input.size() == 1);
    Log_debug("TPCC_NEW_ORDER, piece: %d", TPCC_NEW_ORDER_RI(I));
    mdb::Row *r = dtxn->Query(dtxn->GetTable(TPCC_TB_ITEM),
                              input[TPCC_VAR_I_ID(I)].get_blob(),
                              ROW_ITEM);
    // Ri item
    dtxn->ReadColumn(r,
                     TPCC_COL_ITEM_I_NAME,
                     &output[TPCC_VAR_I_NAME(I)],
                     TXN_BYPASS);
    dtxn->ReadColumn(r,
                     TPCC_COL_ITEM_I_PRICE,
                     &output[TPCC_VAR_I_PRICE(I)],
                     TXN_BYPASS);
    dtxn->ReadColumn(r,
                     TPCC_COL_ITEM_I_DATA,
                     &output[TPCC_VAR_I_DATA(I)],
                     TXN_BYPASS);

    *res = SUCCESS;
    return;
  END_LOOP_PIE


  for (int i = (0); i < (1000); i++) {
    // 1000 is a magical number?
    SHARD_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_RS(i),
              TPCC_TB_STOCK, TPCC_VAR_S_W_ID(i))
    INPUT_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_RS(i),
              TPCC_VAR_D_ID, TPCC_VAR_I_ID(i), TPCC_VAR_S_W_ID(i))
  }

  BEGIN_LOOP_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_RS(0), 1000, DF_NO)
    verify(input.size() == 3);
    Log_debug("TPCC_NEW_ORDER, piece: %d", TPCC_NEW_ORDER_RS(I));
    mdb::MultiBlob mb(2);
    mb[0] = input[TPCC_VAR_I_ID(I)].get_blob();
    mb[1] = input[TPCC_VAR_S_W_ID(I)].get_blob();
    mdb::Row *r = dtxn->Query(dtxn->GetTable(TPCC_TB_STOCK), mb, ROW_STOCK);
    verify(r->schema_);
    //i32 s_dist_col = 3 + input[2].get_i32();
    // Ri stock
    // FIXME compress all s_dist_xx into one column
    dtxn->ReadColumn(r, TPCC_COL_STOCK_S_DIST_XX,
                     &output[TPCC_VAR_OL_DIST_INFO(I)],
                     TXN_BYPASS); // 0 ==> s_dist_xx
    dtxn->ReadColumn(r, TPCC_COL_STOCK_S_DATA,
                     &output[TPCC_VAR_S_DATA(I)],
                     TXN_BYPASS); // 1 ==> s_data
    *res = SUCCESS;
    return;
  END_LOOP_PIE

  for (int i = 0; i < (1000); i++) {
    // 1000 is a magical number?
    SHARD_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_WS(i),
              TPCC_TB_STOCK, TPCC_VAR_S_W_ID(i))
    INPUT_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_WS(i),
              TPCC_VAR_I_ID(i), TPCC_VAR_S_W_ID(i),
              TPCC_VAR_OL_QUANTITY(i), TPCC_VAR_S_REMOTE_CNT(i))
  }

  BEGIN_LOOP_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_WS(0), 1000, DF_REAL)
    verify(input.size() == 4);
    Log_debug("TPCC_NEW_ORDER, piece: %d", TPCC_NEW_ORDER_WS(I));
    mdb::Row *r = NULL;
    mdb::MultiBlob mb(2);
    mb[0] = input[TPCC_VAR_I_ID(I)].get_blob();
    mb[1] = input[TPCC_VAR_S_W_ID(I)].get_blob();

    r = dtxn->Query(dtxn->GetTable(TPCC_TB_STOCK), mb, ROW_STOCK_TEMP);
    verify(r->schema_);

//    RCC_KISS(r, 2, false);
//    RCC_KISS(r, 13, false);
//    RCC_KISS(r, 14, false);
//    RCC_KISS(r, 15, false);
//    RCC_SAVE_ROW(r, TPCC_NEW_ORDER_7);
//    RCC_PHASE1_RET;
//    RCC_LOAD_ROW(r, TPCC_NEW_ORDER_7);

    // Ri stock
    Value buf(0);
    dtxn->ReadColumn(r, TPCC_COL_STOCK_S_QUANTITY, &buf);
    int32_t new_ol_quantity = buf.get_i32() -
        input[TPCC_VAR_OL_QUANTITY(I)].get_i32();

    dtxn->ReadColumn(r, TPCC_COL_STOCK_S_YTD, &buf);
    Value new_s_ytd(buf.get_i32() +
        input[TPCC_VAR_OL_QUANTITY(I)].get_i32());

    dtxn->ReadColumn(r, TPCC_COL_STOCK_S_ORDER_CNT, &buf);
    Value new_s_order_cnt((i32)(buf.get_i32() + 1));

    dtxn->ReadColumn(r, TPCC_COL_STOCK_S_REMOTE_CNT, &buf);
    Value new_s_remote_cnt(buf.get_i32() +
        input[TPCC_VAR_S_REMOTE_CNT(I)].get_i32());

    if (new_ol_quantity < 10)
      new_ol_quantity += 91;
    Value new_ol_quantity_value(new_ol_quantity);

    dtxn->WriteColumns(r,
                       {
                           TPCC_COL_STOCK_S_QUANTITY,
                           TPCC_COL_STOCK_S_YTD,
                           TPCC_COL_STOCK_S_ORDER_CNT,
                           TPCC_COL_STOCK_S_REMOTE_CNT
                       },
                       {
                           new_ol_quantity_value,
                           new_s_ytd,
                           new_s_order_cnt,
                           new_s_remote_cnt
                       });
    *res = SUCCESS;
    return;
  END_LOOP_PIE

  for (int i = 0; i < 1000; i++) {
    // 1000 is a magical number?
    SHARD_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_WOL(i),
              TPCC_TB_ORDER_LINE, TPCC_VAR_W_ID)
    INPUT_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_WOL(i),
              TPCC_VAR_I_ID(i), TPCC_VAR_I_PRICE(i),
              TPCC_VAR_O_ID, TPCC_VAR_S_W_ID(i),
              TPCC_VAR_W_ID, TPCC_VAR_D_ID,
              TPCC_VAR_OL_DIST_INFO(i), TPCC_VAR_OL_QUANTITY(i),
              TPCC_VAR_OL_NUMBER(i), TPCC_VAR_OL_DELIVER_D(i))
  }

  BEGIN_LOOP_PIE(TPCC_NEW_ORDER, TPCC_NEW_ORDER_WOL(0), 1000, DF_REAL) {
    verify(input.size() == 10);
    Log_debug("TPCC_NEW_ORDER, piece: %d", TPCC_NEW_ORDER_WOL(I));

    mdb::Table *tbl = dtxn->GetTable(TPCC_TB_ORDER_LINE);
    mdb::Row *r = NULL;

    Value amount = Value((double) (input[TPCC_VAR_I_PRICE(I)].get_double() *
              input[TPCC_VAR_OL_QUANTITY(I)].get_i32()));

    CREATE_ROW(tbl->schema(), vector<Value>({
      input[TPCC_VAR_D_ID],
      input[TPCC_VAR_W_ID],
      input[TPCC_VAR_O_ID],
      input[TPCC_VAR_OL_NUMBER(I)],
      input[TPCC_VAR_I_ID(I)],
      input[TPCC_VAR_S_W_ID(I)],
      input[TPCC_VAR_OL_DELIVER_D(I)],
      input[TPCC_VAR_OL_QUANTITY(I)],
      amount,
      input[TPCC_VAR_OL_DIST_INFO(I)]
    }));


//    RCC_KISS(r, 0, false);
//    RCC_KISS(r, 1, false);
//    RCC_KISS(r, 2, false);
//    RCC_KISS(r, 3, false);
//    RCC_KISS(r, 4, false);
//    RCC_KISS(r, 6, false);
//    RCC_KISS(r, 8, false);
//    RCC_SAVE_ROW(r, TPCC_NEW_ORDER_8);
//    RCC_PHASE1_RET;
//    RCC_LOAD_ROW(r, TPCC_NEW_ORDER_8);
        
    dtxn->InsertRow(tbl, r);
    *res = SUCCESS;
    return;
  } END_LOOP_PIE
}
} // namespace rococo
