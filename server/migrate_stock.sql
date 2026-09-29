-- =============================================================
-- 이미 만들어 둔 kiosk DB에 재고(stock) 칸을 추가한다. (한 번만 실행)
-- 실행: sudo mysql < server/migrate_stock.sql
-- schema.sql로 새로 만든 DB라면 이미 들어 있으므로 실행하지 않아도 된다.
-- =============================================================
USE kiosk;

ALTER TABLE products ADD COLUMN stock INT NOT NULL DEFAULT 0 AFTER price;

-- 기존 상품들이 바로 품절로 보이지 않도록 재고를 10개로 채운다. (관리자 화면에서 수정 가능)
UPDATE products SET stock = 10;
