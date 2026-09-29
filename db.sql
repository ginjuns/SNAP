###user테이블

##create schema test2;
##use test2;

create table userlist(
userlist_id int primary key auto_increment, 
user_type varchar(10), 
user_id char(18) unique, 
user_password char(18),
phonenumber varchar(13) unique, 
cash int);





###face id 테이블

create table face(
face_id int primary key auto_increment, 

userid int,
foreign key (userid) REFERENCES userlist(userlist_id),

face_vector varchar(2000)
);





###상품 테이블

create table product(
product_id int primary key auto_increment, 
product_name varchar(10),
product_price int default 0,
stock int default 0,
soldout int default 1,
image varchar(3100)
);




###판매 테이블

create table sales(
sales_id int primary key auto_increment,

userid int,
foreign key (userid) REFERENCES userlist(userlist_id),

productid int,
foreign key (productid) references product(product_id),

price int default 0,
quantity int default 1,
datelog datetime
);





##userlist 테이블의 cash를 거래테이블로 참조하기 위해서 이런 식으로 참조하는 조건을 가지게 변경.
ALTER TABLE userlist
ADD INDEX idx_cash (cash);


###거래 테이블

create table trade(
trade_id int primary key auto_increment,

userid int,
foreign key (userid) REFERENCES userlist(userlist_id),

tradedate datetime, 
cash_incr int, 
cash_decr int, 

total_cash int, 
foreign key (total_cash) references userlist(cash)
); 


##face_vector를 벡터값을 받기 좋게 변경
ALTER TABLE face
MODIFY face_vector TEXT;


