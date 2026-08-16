package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
)

type payload struct {
	Hello string `json:"hello"`
	N     int    `json:"n"`
	OK    bool   `json:"ok"`
}

// Benchmark comparison server (Phase 38.2) — mirrors Aurora /hello endpoint.
// Run:  go run bench_http.go   (listens on :8082)
func main() {
	http.HandleFunc("/hello", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(payload{Hello: "world", N: 1, OK: true})
	})
	http.HandleFunc("/text", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintf(w, "hello")
	})
	log.Println("Go benchmark server on :8082")
	log.Fatal(http.ListenAndServe(":8082", nil))
}