#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <stdint.h>
#include <time.h>

#define TOTAL_SIZE (1LL << 30)
#define CHUNK_SIZE (1024 * 1024 * 1024)
#define TMP_PATH "E:\\"
#define TMP_FILENAME "bench.tmp"



uint64_t get_time_ms() {
	// 64bit精度で時間を取得．
	// dwLowDateTimeが下位32ビット，dwHighDateTimeが上位32ビット
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	ULARGE_INTEGER uli; // unsigned int64

	// LowPartが下位32ビット，HighPartが上位32ビット
	// QuadPartがその全体を表す
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
	return uli.QuadPart / 10000; // ミリ秒単位に変換
}

void fill_random_data(char* buf, size_t size) {
	for (size_t i = 0; i < size; i++) {
		buf[i] = (char)(rand() % 256); // 0-255のランダムなバイト値
	}
}

void print_datetime() {
	time_t now = time(NULL);
	struct tm local;
	localtime_s(&local, &now);
	char buf[64];
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local);
	printf("実行日時: %s\n", buf);
}

int main(int argc, char* argv[]) {
	// データサイズの設定
	size_t chunk_size = CHUNK_SIZE;
	int64_t total_size = TOTAL_SIZE;

	char tmp_path[256] = TMP_PATH; // 一時ファイルのパス
	int use_cache = 0; // 1:キャッシュ有効 0:キャッシュ無効

	// コマンドライン引数の処理
	// filename [total_size] [chunk_size] [tmp_path] [use_cache]

	if (argc >= 2) total_size = _strtoi64(argv[1], NULL, 10);  // strtoi64(値, ポインタ, 基数)
	if (argc >= 3) chunk_size = (size_t)atoll(argv[2]);
	if (argc >= 4) strncpy(tmp_path, argv[3], sizeof(tmp_path) - 1);
	if (argc >= 5) use_cache = atoi(argv[4]);
	if (argc >= 6) {
		printf("%s [total_size] [chunk_size] [tmp_path] [use_cache]\n", argv[0]);
		return 1;
	}

	char file_path[MAX_PATH];
	// snprintf: バッファに文字列を書き込む．バッファサイズ指定が必要
	snprintf(file_path, sizeof(file_path), "%s%s", tmp_path, TMP_FILENAME);

	char* buf = (char*)malloc(chunk_size);
	if (!buf) {
		fprintf(stderr, "メモリ確保失敗\n");
		return 1;
	}
	fill_random_data(buf, chunk_size);

	// ファイルのフラグ設定
	// use_cacheが0ならキャッシュを無効にする
	DWORD flags = use_cache ? FILE_ATTRIBUTE_NORMAL : (FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH);
	printf("=== スピードテスト ===\n");
	printf("ファイルパス: %s\n", file_path);
	printf("書込サイズ: %lld バイト (%lldメガバイト)\n", (long long)total_size, (long long)(total_size >> 20));
	printf("チャンクサイズ: %zu バイト (%lldメガバイト)\n", chunk_size, (long long)chunk_size >> 20); // size_t型は%zuを使用
	printf("キャッシュ%s\n", use_cache ? "有効" : "無効");
	


	// ファイル書込
	HANDLE hFile = CreateFileA(file_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, flags, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "ファイルオープン失敗: %lu\n", GetLastError());
		free(buf);
		return 1;
	}

	uint64_t start_write_time = get_time_ms();
	int64_t total_written = 0;
	DWORD written;

	while (total_written < total_size) {
		size_t to_write = (total_size - total_written < chunk_size) ? (total_size - total_written) : chunk_size;
		if (!WriteFile(hFile, buf, (DWORD)to_write, &written, NULL)) {
			fprintf(stderr, "書込失敗: %lu\n", GetLastError());
			CloseHandle(hFile);
			free(buf);
			DeleteFileW(file_path); // 書込失敗時はファイルを削除
			return 1;
		}
		total_written += to_write;
	}

	FlushFileBuffers(hFile); // バッファを強制書込
	uint64_t end_write_time = get_time_ms();
	CloseHandle(hFile);

	double write_time = (end_write_time - start_write_time) / 1000.0;
	double write_mb_per_sec = (total_size / (1 << 20)) / write_time; // MB/s

	printf("書き込み: %.3f MB/s\n書込時間: %.3f s\n", write_mb_per_sec, write_time);

	// ファイル読み込み

	hFile = CreateFileA(file_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, flags, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "ファイルオープン失敗: %lu\n", GetLastError());
		free(buf);
		return 1;
	}

	uint64_t start_read_time = get_time_ms();
	int64_t total_read = 0;
	DWORD read = 0;

	while (total_read < total_size) {
		size_t to_read = (total_size - total_read < chunk_size) ? (total_size - total_read) : chunk_size;
		if (!ReadFile(hFile, buf, (DWORD)to_read, &read, NULL) || read != (DWORD)to_read) {
			fprintf(stderr, "読み込み失敗: %lu\n", GetLastError());
			CloseHandle(hFile);
			free(buf);
			DeleteFileW(file_path);
			return 1;
		}
		total_read += to_read;
	}
	CloseHandle(hFile);
	uint64_t end_read_time = get_time_ms();
	double read_time = (end_read_time - start_read_time) / 1000.0;
	double read_mb_per_sec = (total_size / (1 << 20)) / read_time; // MB/s
	printf("読み込み: %.3f MB/s\n読み込み時間: %.3f s\n", read_mb_per_sec, read_time);
	
	free(buf);
	DeleteFileA(file_path);
	printf("テスト終了\n");
	print_datetime();
	return 0;


}