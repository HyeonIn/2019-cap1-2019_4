'''
train set 에 맞는 y값 (분류) 만드는 함수
'''

def make_answer(train_x):
    
    SQL = "SELECT order_id, user_id FROM orders"
    orders_df = pd.read_sql(SQL, db)

    SQL = "SELECT order_id, product_id, reordered FROM order_products__train"
    train_df = pd.read_sql(SQL, db)
    
    answer = pd.merge(train_df, orders_df, how="inner", on="order_id")
    
    del orders_df, train_df
    
    #order_id 제거
    answer = answer[["user_id", "product_id", "reordered"]]
    
    # train과 그 외 정보를  merge
    train_df = pd.merge(train_result(), answer, how="left", on=["user_id", "product_id"])
    
    del answer
    
    # reordered 값이 nan 인것들은 0으로 변경
    train_df["reordered"].fillna(0, inplace=True)
    
    train_y = train_df.reordered.values
    return train_y

