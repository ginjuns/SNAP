-- =============================================================
-- 얼굴인식 키오스크 DB (MySQL 5.7 / 8.0)
-- 실행: sudo mysql < db/kiosk.sql
--
-- 처음 설치할 때도, 예전 버전 DB를 최신으로 맞출 때도 이 파일 하나만 실행하면 된다.
-- 여러 번 실행해도 기존 데이터는 지워지지 않는다.
-- =============================================================

CREATE DATABASE IF NOT EXISTS kiosk
    DEFAULT CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;     -- _ci: 대소문자 구분 없음 (상품명 중복 판정에도 적용)
USE kiosk;

-- -------------------------------------------------------------
-- 1. 테이블
-- -------------------------------------------------------------

-- 사용자: id = 얼굴인식 결과(LBPH 라벨)
CREATE TABLE IF NOT EXISTS users (
    id          INT          NOT NULL AUTO_INCREMENT,
    name        VARCHAR(50)  NOT NULL,
    role        ENUM('admin', 'member') NOT NULL DEFAULT 'member',
    balance     INT          NOT NULL DEFAULT 0,          -- 충전 잔액(원)
    login_id    VARCHAR(30)  NULL,                        -- 비밀번호 로그인용 아이디 (얼굴인식 실패 시)
    password    VARCHAR(100) COLLATE utf8mb4_bin NULL,    -- 비밀번호 (_bin: 대소문자 구분)
    created_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uq_users_login_id (login_id)
) ENGINE=InnoDB;

-- 상품: 사진은 MEDIUMBLOB(최대 16MB)에 저장. BLOB(64KB)은 사진이 잘리므로 사용 금지
CREATE TABLE IF NOT EXISTS products (
    id          INT          NOT NULL AUTO_INCREMENT,
    name        VARCHAR(100) NOT NULL,
    price       INT          NOT NULL,
    stock       INT          NOT NULL DEFAULT 0,          -- 재고 수량 (결제 시 차감)
    shelf       INT          NULL,                        -- 진열대 번호 (1~6, NULL = 지정 안 함)
    image      MEDIUMBLOB   NULL,
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

-- 얼굴 사진: 앱 회원가입(REGISTER) 때 저장, 키오스크가 FACES로 받아 학습한다.
-- 사용자 1명당 여러 장. 사용자를 지우면 사진도 같이 지워진다.
CREATE TABLE IF NOT EXISTS face_images (
    id          INT          NOT NULL AUTO_INCREMENT,
    user_id     INT          NOT NULL,
    image       MEDIUMBLOB   NOT NULL,                    -- JPEG/PNG 원본
    created_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    CONSTRAINT fk_face_images_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- -------------------------------------------------------------
-- 2. 예전 버전 DB 업데이트 (이미 최신이면 아무것도 하지 않음)
--    MySQL은 "칸이 없을 때만 추가"를 바로 쓸 수 없어서, 칸이 있는지 확인한 뒤 실행한다.
-- -------------------------------------------------------------

-- 재고(stock) 칸 추가. 기존 상품이 바로 품절로 보이지 않게 10개로 채운다.
SET @sql = IF((SELECT COUNT(*) FROM information_schema.columns
               WHERE table_schema = 'kiosk' AND table_name = 'products' AND column_name = 'stock') = 0,
              'ALTER TABLE products ADD COLUMN stock INT NOT NULL DEFAULT 10 AFTER price',
              'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
ALTER TABLE products ALTER COLUMN stock SET DEFAULT 0;

-- 진열대 번호(shelf) 칸 추가. 기존 상품은 지정 안 함(NULL)
SET @sql = IF((SELECT COUNT(*) FROM information_schema.columns
               WHERE table_schema = 'kiosk' AND table_name = 'products' AND column_name = 'shelf') = 0,
              'ALTER TABLE products ADD COLUMN shelf INT NULL AFTER stock',
              'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- 비밀번호 로그인용 아이디(login_id) 칸 추가
SET @sql = IF((SELECT COUNT(*) FROM information_schema.columns
               WHERE table_schema = 'kiosk' AND table_name = 'users' AND column_name = 'login_id') = 0,
              'ALTER TABLE users ADD COLUMN login_id VARCHAR(30) NULL AFTER balance, ADD UNIQUE KEY uq_users_login_id (login_id)',
              'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- 비밀번호(password) 칸 추가 (글자 그대로 저장)
SET @sql = IF((SELECT COUNT(*) FROM information_schema.columns
               WHERE table_schema = 'kiosk' AND table_name = 'users' AND column_name = 'password') = 0,
              'ALTER TABLE users ADD COLUMN password VARCHAR(100) COLLATE utf8mb4_bin NULL AFTER login_id',
              'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- 예전에 암호화(SHA2)해서 저장하던 password_hash 칸 삭제
-- (암호화된 값은 되돌릴 수 없으므로 샘플 계정 외 회원은 비밀번호를 다시 넣어야 한다. 맨 아래 예 참고)
SET @sql = IF((SELECT COUNT(*) FROM information_schema.columns
               WHERE table_schema = 'kiosk' AND table_name = 'users' AND column_name = 'password_hash') > 0,
              'ALTER TABLE users DROP COLUMN password_hash',
              'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- -------------------------------------------------------------
-- 3. 샘플 데이터 (이미 있으면 건너뜀)
--    아이디 / 비밀번호: admin / admin1234, hong / hong1234, kim / kim1234
-- -------------------------------------------------------------
INSERT IGNORE INTO users (id, name, role, balance, login_id, password) VALUES
    (1, '관리자', 'admin',  0,     'admin', 'admin1234'),
    (2, '홍길동', 'member', 50000, 'hong',  'hong1234'),
    (3, '김철수', 'member', 30000, 'kim',   'kim1234');

-- 예전 DB의 샘플 계정에 아이디/비밀번호가 비어 있으면 채운다.
UPDATE users SET login_id = 'admin' WHERE id = 1 AND login_id IS NULL;
UPDATE users SET login_id = 'hong'  WHERE id = 2 AND login_id IS NULL;
UPDATE users SET login_id = 'kim'   WHERE id = 3 AND login_id IS NULL;
UPDATE users SET password = 'admin1234' WHERE login_id = 'admin' AND password IS NULL;
UPDATE users SET password = 'hong1234'  WHERE login_id = 'hong'  AND password IS NULL;
UPDATE users SET password = 'kim1234'   WHERE login_id = 'kim'   AND password IS NULL;

INSERT IGNORE INTO products (name, price, stock) VALUES
    ('아메리카노', 3000, 20),
    ('카페라떼',   3500, 20),
    ('샌드위치',   5000, 10);

-- -------------------------------------------------------------
-- 4. 서버 접속 계정 (비밀번호는 바꿔서 쓰고 qt/server/server.ini 에도 같게 입력)
-- -------------------------------------------------------------
CREATE USER IF NOT EXISTS 'kiosk'@'localhost' IDENTIFIED BY 'kiosk1234';
GRANT SELECT, INSERT, UPDATE, DELETE ON kiosk.* TO 'kiosk'@'localhost';
FLUSH PRIVILEGES;

-- 다른 회원의 비밀번호를 넣거나 바꾸는 예:
--   UPDATE users SET password = 'jun1234' WHERE login_id = 'jun';
-- 회원 잔액 충전 예:
--   UPDATE users SET balance = balance + 10000 WHERE login_id = 'jun';
