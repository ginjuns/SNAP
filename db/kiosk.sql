CREATE DATABASE IF NOT EXISTS kiosk
    DEFAULT CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;
USE kiosk;

-- 사용자
CREATE TABLE IF NOT EXISTS users (
    id          INT          NOT NULL AUTO_INCREMENT,
    name        VARCHAR(50)  NOT NULL,
    role        ENUM('admin', 'member') NOT NULL DEFAULT 'member',
    balance     INT          NOT NULL DEFAULT 0,
    login_id    VARCHAR(30)  NULL,
    password    VARCHAR(100) COLLATE utf8mb4_bin NULL,
    phone       VARCHAR(20)  NULL,
    created_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uq_users_login_id (login_id),
    UNIQUE KEY uq_users_phone (phone)
) ENGINE=InnoDB;

-- 상품
CREATE TABLE IF NOT EXISTS products (
    id          INT          NOT NULL AUTO_INCREMENT,
    name        VARCHAR(100) NOT NULL,
    price       INT          NOT NULL,
    stock       INT          NOT NULL DEFAULT 0,
    shelf       INT          NULL,
    image      MEDIUMBLOB   NULL,
    updated_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uq_products_name (name)
) ENGINE=InnoDB;

-- 매출
CREATE TABLE IF NOT EXISTS sales (
    id            BIGINT       NOT NULL AUTO_INCREMENT,
    user_id       INT          NULL,
    product_id    INT          NULL,
    product_name  VARCHAR(100) NOT NULL,
    qty           INT          NOT NULL,
    amount        INT          NOT NULL,
    method        ENUM('card', 'face') NOT NULL,
    sold_at       DATETIME     NOT NULL,
    PRIMARY KEY (id),
    KEY idx_sales_sold_at (sold_at),
    CONSTRAINT fk_sales_user    FOREIGN KEY (user_id)    REFERENCES users (id)    ON DELETE SET NULL,
    CONSTRAINT fk_sales_product FOREIGN KEY (product_id) REFERENCES products (id) ON DELETE SET NULL
) ENGINE=InnoDB;

-- 얼굴 사진
CREATE TABLE IF NOT EXISTS face_images (
    id          INT          NOT NULL AUTO_INCREMENT,
    user_id     INT          NOT NULL,
    image       MEDIUMBLOB   NOT NULL,
    created_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    CONSTRAINT fk_face_images_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- 충전 기록
CREATE TABLE IF NOT EXISTS charges (
    id          INT          NOT NULL AUTO_INCREMENT,
    user_id     INT          NOT NULL,
    amount      INT          NOT NULL,
    payment_key VARCHAR(200) NULL,
    order_id    VARCHAR(64)  NULL,
    charged_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_charges_user (user_id, charged_at),
    UNIQUE KEY uq_charges_payment_key (payment_key),
    CONSTRAINT fk_charges_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- 카드
CREATE TABLE IF NOT EXISTS cards (
    uid         VARCHAR(32)  NOT NULL,
    name        VARCHAR(50)  NOT NULL,
    balance     INT          NOT NULL DEFAULT 0,
    created_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (uid)
) ENGINE=InnoDB;

-- 서버 접속 계정
CREATE USER IF NOT EXISTS 'kiosk'@'localhost' IDENTIFIED BY 'kiosk1234';
GRANT SELECT, INSERT, UPDATE, DELETE ON kiosk.* TO 'kiosk'@'localhost';
FLUSH PRIVILEGES;
