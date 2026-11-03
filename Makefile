.PHONY: dev artifact

artifact: dev
	docker compose exec -T dev sh /usr/local/bin/imetoggle-build

dev:
	docker compose up -d --build dev
