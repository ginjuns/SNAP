-- =============================================================
-- 얼굴인식 키오스크 DB (MySQL 5.7 / 8.0)
-- 실행: mysql -u root -p < schema.sql
-- 여러 번 실행해도 기존 데이터는 지워지지 않는다.
-- =============================================================

CREATE DATABASE IF NOT EXISTS kiosk
    DEFAULT CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;     -- _ci: 대소문자 구분 없음 (상품명 중복 판정에도 적용)
USE kiosk;

-- 사용자: id = 얼굴인식 학습 폴더 이름(faces/<id>)
CREATE TABLE IF NOT EXISTS users (
    id          INT          NOT NULL AUTO_INCREMENT,
    name        VARCHAR(50)  NOT NULL,
    role        ENUM('admin', 'member') NOT NULL DEFAULT 'member',
    balance     INT          NOT NULL DEFAULT 0,          -- 충전 잔액(원)
    created_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id)
) ENGINE=InnoDB;

-- 상품: 사진은 MEDIUMBLOB(최대 16MB)에 저장. BLOB(64KB)은 사진이 잘리므로 사용 금지
CREATE TABLE IF NOT EXISTS products (
    id          INT          NOT NULL AUTO_INCREMENT,
    name        VARCHAR(100) NOT NULL,
    price       INT          NOT NULL,
    stock       INT          NOT NULL DEFAULT 0,          -- 재고 수량 (결제 시 차감)
    image       MEDIUMBLOB   NULL,
    updated_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uq_products_name (name)                    -- 상품명 중복 금지
) ENGINE=InnoDB;

-- 매출: 결제 1건의 상품 1줄 = 1행.
-- 상품/사용자가 삭제돼도 기록이 남도록 이름과 금액을 따로 저장하고 FK는 NULL로 바꾼다.
CREATE TABLE IF NOT EXISTS sales (
    id            BIGINT       NOT NULL AUTO_INCREMENT,
    user_id       INT          NULL,
    product_id    INT          NULL,
    product_name  VARCHAR(100) NOT NULL,
    qty           INT          NOT NULL,
    amount        INT          NOT NULL,                  -- 단가 * 수량
    method        ENUM('card', 'face') NOT NULL,
    sold_at       DATETIME     NOT NULL,
    PRIMARY KEY (id),
    KEY idx_sales_sold_at (sold_at),
    CONSTRAINT fk_sales_user    FOREIGN KEY (user_id)    REFERENCES users (id)    ON DELETE SET NULL,
    CONSTRAINT fk_sales_product FOREIGN KEY (product_id) REFERENCES products (id) ON DELETE SET NULL
) ENGINE=InnoDB;

-- 샘플 데이터 (이미 있으면 건너뜀)
INSERT IGNORE INTO users (id, name, role, balance) VALUES
    (1, '관리자', 'admin',  0),
    (2, '홍길동', 'member', 50000),
    (3, '김철수', 'member', 30000);

INSERT IGNORE INTO products (name, price, stock) VALUES
    ('아메리카노', 3000, 20),
    ('카페라떼',   3500, 20),
    ('샌드위치',   5000, 10);

-- 서버 접속 계정 (비밀번호는 바꿔서 쓰고 server.ini에도 같게 입력)
CREATE USER IF NOT EXISTS 'kiosk'@'localhost' IDENTIFIED BY 'kiosk1234';
GRANT SELECT, INSERT, UPDATE, DELETE ON kiosk.* TO 'kiosk'@'localhost';
FLUSH PRIVILEGES;
