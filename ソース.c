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
	// 64bit���x�Ŏ��Ԃ��擾�D
	// dwLowDateTime������32�r�b�g�CdwHighDateTime�����32�r�b�g
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	ULARGE_INTEGER uli; // unsigned int64

	// LowPart������32�r�b�g�CHighPart�����32�r�b�g
	// QuadPart�����̑S�̂�\��
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
	return uli.QuadPart / 10000; // �~���b�P�ʂɕϊ�
}

void fill_random_data(char* buf, size_t size) {
	for (size_t i = 0; i < size; i++) {
		buf[i] = (char)(rand() % 256); // 0-255�̃����_���ȃo�C�g�l
	}
}

void print_datetime() {
	time_t now = time(NULL);
	struct tm local;
	localtime_s(&local, &now);
	char buf[64];
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local);
	printf("���s����: %s\n", buf);
}

int main(int argc, char* argv[]) {
	// �f�[�^�T�C�Y�̐ݒ�
	size_t chunk_size = CHUNK_SIZE;
	int64_t total_size = TOTAL_SIZE;

	char tmp_path[256] = TMP_PATH; // �ꎞ�t�@�C���̃p�X
	int use_cache = 0; // 1:�L���b�V���L�� 0:�L���b�V������

	// �R�}���h���C�������̏���
	// filename [total_size] [chunk_size] [tmp_path] [use_cache]

	if (argc >= 2) total_size = _strtoi64(argv[1], NULL, 10);  // strtoi64(�l, �|�C���^, �)
	if (argc >= 3) chunk_size = (size_t)atoll(argv[2]);
	if (argc >= 4) strncpy(tmp_path, argv[3], sizeof(tmp_path) - 1);
	if (argc >= 5) use_cache = atoi(argv[4]);
	if (argc >= 6) {
		printf("%s [total_size] [chunk_size] [tmp_path] [use_cache]\n", argv[0]);
		return 1;
	}

	char file_path[MAX_PATH];
	// snprintf: �o�b�t�@�ɕ�������������ށD�o�b�t�@�T�C�Y�w�肪�K�v
	snprintf(file_path, sizeof(file_path), "%s%s", tmp_path, TMP_FILENAME);

	char* buf = (char*)malloc(chunk_size);
	if (!buf) {
		fprintf(stderr, "�������m�ێ��s\n");
		return 1;
	}
	fill_random_data(buf, chunk_size);

	// �t�@�C���̃t���O�ݒ�
	// use_cache��0�Ȃ�L���b�V���𖳌��ɂ���
	DWORD flags = use_cache ? FILE_ATTRIBUTE_NORMAL : (FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH);
	printf("=== �X�s�[�h�e�X�g ===\n");
	printf("�t�@�C���p�X: %s\n", file_path);
	printf("�����T�C�Y: %lld �o�C�g (%lld���K�o�C�g)\n", (long long)total_size, (long long)(total_size >> 20));
	printf("�`�����N�T�C�Y: %zu �o�C�g (%lld���K�o�C�g)\n", chunk_size, (long long)chunk_size >> 20); // size_t�^��%zu���g�p
	printf("�L���b�V��%s\n", use_cache ? "�L��" : "����");
	


	// �t�@�C������
	HANDLE hFile = CreateFileA(file_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, flags, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "�t�@�C���I�[�v�����s: %lu\n", GetLastError());
		free(buf);
		return 1;
	}

	uint64_t start_write_time = get_time_ms();
	int64_t total_written = 0;
	DWORD written;

	while (total_written < total_size) {
		size_t to_write = (total_size - total_written < chunk_size) ? (total_size - total_written) : chunk_size;
		if (!WriteFile(hFile, buf, (DWORD)to_write, &written, NULL)) {
			fprintf(stderr, "�������s: %lu\n", GetLastError());
			CloseHandle(hFile);
			free(buf);
			DeleteFileW(file_path); // �������s���̓t�@�C�����폜
			return 1;
		}
		total_written += to_write;
	}

	FlushFileBuffers(hFile); // �o�b�t�@����������
	uint64_t end_write_time = get_time_ms();
	CloseHandle(hFile);

	double write_time = (end_write_time - start_write_time) / 1000.0;
	double write_mb_per_sec = (total_size / (1 << 20)) / write_time; // MB/s

	printf("��������: %.3f MB/s\n��������: %.3f s\n", write_mb_per_sec, write_time);

	// �t�@�C���ǂݍ���

	hFile = CreateFileA(file_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, flags, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "�t�@�C���I�[�v�����s: %lu\n", GetLastError());
		free(buf);
		return 1;
	}

	uint64_t start_read_time = get_time_ms();
	int64_t total_read = 0;
	DWORD read = 0;

	while (total_read < total_size) {
		size_t to_read = (total_size - total_read < chunk_size) ? (total_size - total_read) : chunk_size;
		if (!ReadFile(hFile, buf, (DWORD)to_read, &read, NULL) || read != (DWORD)to_read) {
			fprintf(stderr, "�ǂݍ��ݎ��s: %lu\n", GetLastError());
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
	printf("�ǂݍ���: %.3f MB/s\n�ǂݍ��ݎ���: %.3f s\n", read_mb_per_sec, read_time);
	
	free(buf);
	DeleteFileA(file_path);
	printf("�e�X�g�I��\n");
	print_datetime();
	return 0;


}