-- =============================================================
-- 암호화(SHA2)해 저장하던 비밀번호를 글자 그대로 저장하도록 바꾼다. (한 번만 실행)
-- 실행: sudo mysql < server/migrate_plain_password.sql
-- 암호화된 값은 원래 비밀번호로 되돌릴 수 없으므로
-- 샘플 계정 외의 회원은 아래 예처럼 비밀번호를 다시 넣어야 한다.
-- =============================================================
USE kiosk;

ALTER TABLE users
    ADD COLUMN password VARCHAR(100) COLLATE utf8mb4_bin NULL AFTER login_id,
    DROP COLUMN password_hash;

UPDATE users SET password = 'admin1234' WHERE login_id = 'admin';
UPDATE users SET password = 'hong1234'  WHERE login_id = 'hong';
UPDATE users SET password = 'kim1234'   WHERE login_id = 'kim';

-- 다른 회원 비밀번호 다시 넣기 예:
--   UPDATE users SET password = 'jun1234' WHERE login_id = 'jun';
