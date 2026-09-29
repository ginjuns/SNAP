-- =============================================================
-- 이미 만들어 둔 kiosk DB에 비밀번호 로그인 칸을 추가한다. (한 번만 실행)
-- 실행: sudo mysql < server/migrate_password.sql
-- schema.sql로 새로 만든 DB라면 이미 들어 있으므로 실행하지 않아도 된다.
-- =============================================================
USE kiosk;

ALTER TABLE users
    ADD COLUMN login_id VARCHAR(30) NULL AFTER balance,
    ADD COLUMN password_hash CHAR(64) NULL AFTER login_id,
    ADD UNIQUE KEY uq_users_login_id (login_id);

-- 샘플 사용자 아이디/비밀번호: admin / admin1234, hong / hong1234, kim / kim1234
UPDATE users SET login_id = 'admin', password_hash = SHA2('admin:admin1234', 256) WHERE id = 1;
UPDATE users SET login_id = 'hong', password_hash = SHA2('hong:hong1234', 256)   WHERE id = 2;
UPDATE users SET login_id = 'kim', password_hash = SHA2('kim:kim1234', 256)     WHERE id = 3;

-- 다른 사용자 비밀번호 설정 예:
--   UPDATE users SET login_id = 'lee', password_hash = SHA2('lee:비밀번호', 256) WHERE id = 4;
