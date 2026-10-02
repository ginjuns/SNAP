-- =============================================================
-- 이미 만들어 둔 kiosk DB에 얼굴 사진 테이블을 추가한다. (한 번만 실행)
-- 실행: sudo mysql < server/migrate_face.sql
-- 여러 번 실행해도 문제없다.
-- =============================================================
USE kiosk;

CREATE TABLE IF NOT EXISTS face_images (
    id          INT          NOT NULL AUTO_INCREMENT,
    user_id     INT          NOT NULL,
    image       MEDIUMBLOB   NOT NULL,                    -- JPEG/PNG 원본
    created_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    CONSTRAINT fk_face_images_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
) ENGINE=InnoDB;
