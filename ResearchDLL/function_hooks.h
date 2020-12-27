/*
Copyright 2018 Jari J‰‰skel‰

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/


#pragma once
#include <Windows.h>
#include <functional>
#include <assert.h>
#include <unordered_map>
#include <memory>
#include <TlHelp32.h>


namespace fn_hooks {
	typedef std::vector<uint8_t> ByteVector;

	class FunctionBuffer {

	public:
		FunctionBuffer() {}


		/// <summary>Write data to end of the buffer</summary>
		/// <param="bytes">Data to be written</param>
		void Write(const ByteVector &bytes) {
			const size_t size = bytes.size();
			bytes_.resize(bytes_.size() + size);
			memcpy(bytes_.data() + bytes_.size() - size, bytes.data(), size);
		}


		const ByteVector& bytes() const {
			return bytes_;
		}

		const uint8_t* data() const {
			return bytes_.data();
		}

		const size_t size() const {
			return bytes_.size();
		}

		/// <summary>Set memory as executable</summary>
		/// <returns>true if succeed. false otherwise.</returns>
		bool SetAsExecutable() {
			DWORD old_flag = 0;
			return VirtualProtect(reinterpret_cast<void*>(bytes_.data()), bytes_.size(), PAGE_EXECUTE_READWRITE, &old_flag);
		}

	private:
		ByteVector bytes_;
	};

	/// <summary>Get thread ids of the current process</summary>
	/// <param name="thread_ids">Pointer to variable that holds results </param>
	/// <returns>true if succeed. false otherwise.</returns>
	bool GetThreads(std::vector<uint32_t> *thread_ids) {
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, NULL);
		if (snapshot == INVALID_HANDLE_VALUE) {
			return false;
		}

		THREADENTRY32 te32{};
		te32.dwSize = sizeof(THREADENTRY32);

		if (!Thread32First(snapshot, &te32)) {
			CloseHandle(snapshot);
			return false;
		}

		// Iterate over thread list
		do {
			thread_ids->push_back(te32.th32ThreadID);
		} while (Thread32Next(snapshot, &te32));

		CloseHandle(snapshot);
		return true;
	}


	/// <summary>Iterate over threads in the current process</summary>
	/// <param name="cb">Function that receives thread id. Return false to stop.</param>
	/// <returns>true if succeed. false otherwise.</returns>
	bool ThreadForEach(const std::function<bool(const uint32_t th_id)> &cb) {
		std::vector<uint32_t> th_ids;
		if (!GetThreads(&th_ids)) {
			return false;
		}

		for (const uint32_t &th_id : th_ids) {
			if (!cb(th_id)) {
				return false;
			}
		}
		return true;
	}

	/// <summary>Resume threads in the current process</summary>
	/// <returns>true if succeed. false otherwise.</returns>
	bool ResumeThreads() {
		uint32_t curr_th_id = GetCurrentThreadId();

		return ThreadForEach([curr_th_id](const uint32_t th_id) {
			// Skip current thread
			if (th_id == curr_th_id) {
				return true;
			}

			HANDLE th = OpenThread(THREAD_SUSPEND_RESUME, false, th_id);
			if (th == INVALID_HANDLE_VALUE) {
				return false;
			}

			ResumeThread(th);
			CloseHandle(th);
			return true;
		});
	}

	/// <summary>Suspend threads in the current process</summary>
	/// <returns>true if succeed. false otherwise.</returns>
	bool SuspendThreads() {
		uint32_t curr_th_id = GetCurrentThreadId();

		return ThreadForEach([curr_th_id](const uint32_t th_id) {
			// Skip current thread
			if (th_id == curr_th_id) {
				return true;
			}

			HANDLE th = OpenThread(THREAD_SUSPEND_RESUME, false, th_id);
			if (th == INVALID_HANDLE_VALUE) {
				return false;
			}

			SuspendThread(th);
			CloseHandle(th);
			return true;
		});
	}


	struct Hook {
		Hook() : fb(std::make_unique<FunctionBuffer>()) {}

		uint64_t fn_ptr;
		std::unique_ptr<FunctionBuffer> fb;
		ByteVector orig_bytes;
	};

#pragma pack(push)
	struct Registers {
		uint64_t R15;
		uint64_t R14;
		uint64_t R13;
		uint64_t R12;
		uint64_t R11;
		uint64_t R10;
		uint64_t R9;
		uint64_t R8;
		uint64_t RSI;
		uint64_t RBP;
		uint64_t RBX;
		uint64_t RDX;
		uint64_t RCX;
		uint64_t RAX;
	};
#pragma pack(pop)

	/*
	Save XMM registers on stack
	mov rbp,rsp
	and rsp,-32
	lea rsp,[rsp-32*16]
	vmovdqa [rsp+32*0],ymm0
	vmovdqa [rsp+32*1],ymm1
	vmovdqa [rsp+32*2],ymm2
	vmovdqa [rsp+32*3],ymm3
	vmovdqa [rsp+32*4],ymm4
	vmovdqa [rsp+32*5],ymm5
	vmovdqa [rsp+32*6],ymm6
	vmovdqa [rsp+32*7],ymm7
	vmovdqa [rsp+32*8],ymm8
	vmovdqa [rsp+32*9],ymm9
	vmovdqa [rsp+32*10],ymm10
	vmovdqa [rsp+32*11],ymm11
	vmovdqa [rsp+32*12],ymm12
	vmovdqa [rsp+32*13],ymm13
	vmovdqa [rsp+32*14],ymm14
	vmovdqa [rsp+32*15],ymm15
	*/
	const ByteVector kSaveXMMRegs {
		0x48, 0x89, 0xE5, 0x48, 0x83, 0xE4, 0xE0, 0x48, 0x8D, 0xA4, 0x24,
		0x00, 0xFE, 0xFF, 0xFF, 0xC5, 0xFD, 0x7F, 0x04, 0x24, 0xC5, 0xFD,
		0x7F, 0x4C, 0x24, 0x20, 0xC5, 0xFD, 0x7F, 0x54, 0x24, 0x40, 0xC5,
		0xFD, 0x7F, 0x5C, 0x24, 0x60, 0xC5, 0xFD, 0x7F, 0xA4, 0x24, 0x80,
		0x00, 0x00, 0x00, 0xC5, 0xFD, 0x7F, 0xAC, 0x24, 0xA0, 0x00, 0x00,
		0x00, 0xC5, 0xFD, 0x7F, 0xB4, 0x24, 0xC0, 0x00, 0x00, 0x00, 0xC5,
		0xFD, 0x7F, 0xBC, 0x24, 0xE0, 0x00, 0x00, 0x00, 0xC5, 0x7D, 0x7F,
		0x84, 0x24, 0x00, 0x01, 0x00, 0x00, 0xC5, 0x7D, 0x7F, 0x8C, 0x24,
		0x20, 0x01, 0x00, 0x00, 0xC5, 0x7D, 0x7F, 0x94, 0x24, 0x40, 0x01,
		0x00, 0x00, 0xC5, 0x7D, 0x7F, 0x9C, 0x24, 0x60, 0x01, 0x00, 0x00,
		0xC5, 0x7D, 0x7F, 0xA4, 0x24, 0x80, 0x01, 0x00, 0x00, 0xC5, 0x7D,
		0x7F, 0xAC, 0x24, 0xA0, 0x01, 0x00, 0x00, 0xC5, 0x7D, 0x7F, 0xB4,
		0x24, 0xC0, 0x01, 0x00, 0x00, 0xC5, 0x7D, 0x7F, 0xBC, 0x24, 0xE0,
		0x01, 0x00, 0x00
	};

	/*
	Restore XMM registers from stack
	vmovdqa ymm15,[rsp+32*15]
	vmovdqa ymm14,[rsp+32*14]
	vmovdqa ymm13,[rsp+32*13]
	vmovdqa ymm12,[rsp+32*12]
	vmovdqa ymm11,[rsp+32*11]
	vmovdqa ymm10,[rsp+32*10]
	vmovdqa ymm9,[rsp+32*9]
	vmovdqa ymm8,[rsp+32*8]
	vmovdqa ymm7,[rsp+32*7]
	vmovdqa ymm6,[rsp+32*6]
	vmovdqa ymm5,[rsp+32*5]
	vmovdqa ymm4,[rsp+32*4]
	vmovdqa ymm3,[rsp+32*3]
	vmovdqa ymm2,[rsp+32*2]
	vmovdqa ymm1,[rsp+32*1]
	vmovdqa ymm0,[rsp+32*0]
	mov rsp,rbp
	*/
	const ByteVector kRestoreXMMRegs {
		0xC5, 0x7D, 0x6F, 0xBC, 0x24, 0xE0, 0x01, 0x00, 0x00, 0xC5, 0x7D,
		0x6F, 0xB4, 0x24, 0xC0, 0x01, 0x00, 0x00, 0xC5, 0x7D, 0x6F, 0xAC,
		0x24, 0xA0, 0x01, 0x00, 0x00, 0xC5, 0x7D, 0x6F, 0xA4, 0x24, 0x80,
		0x01, 0x00, 0x00, 0xC5, 0x7D, 0x6F, 0x9C, 0x24, 0x60, 0x01, 0x00,
		0x00, 0xC5, 0x7D, 0x6F, 0x94, 0x24, 0x40, 0x01, 0x00, 0x00, 0xC5,
		0x7D, 0x6F, 0x8C, 0x24, 0x20, 0x01, 0x00, 0x00, 0xC5, 0x7D, 0x6F,
		0x84, 0x24, 0x00, 0x01, 0x00, 0x00, 0xC5, 0xFD, 0x6F, 0xBC, 0x24,
		0xE0, 0x00, 0x00, 0x00, 0xC5, 0xFD, 0x6F, 0xB4, 0x24, 0xC0, 0x00,
		0x00, 0x00, 0xC5, 0xFD, 0x6F, 0xAC, 0x24, 0xA0, 0x00, 0x00, 0x00,
		0xC5, 0xFD, 0x6F, 0xA4, 0x24, 0x80, 0x00, 0x00, 0x00, 0xC5, 0xFD,
		0x6F, 0x5C, 0x24, 0x60, 0xC5, 0xFD, 0x6F, 0x54, 0x24, 0x40, 0xC5,
		0xFD, 0x6F, 0x4C, 0x24, 0x20, 0xC5, 0xFD, 0x6F, 0x04, 0x24, 0x48,
		0x89, 0xEC
	};


	/*
	PUSH RAX
	PUSH RCX
	PUSH RDX
	PUSH RBX
	PUSH RBP
	PUSH RSI
	PUSH R8
	PUSH R9
	PUSH R10
	PUSH R11
	PUSH R12
	PUSH R13
	PUSH R14
	PUSH R15
	*/
	const ByteVector kPushGeneralPurposeRegs {
		0x50, 0x51, 0x52, 0x53, 0x55, 0x56, 0x41, 0x50, 0x41, 0x51, 0x41,
		0x52, 0x41, 0x53, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57
	};

	/*
	POP R15
	POP R14
	POP R13
	POP R12
	POP R11
	POP R10
	POP R9
	POP R8
	POP RSI
	POP RBP
	POP RBX
	POP RDX
	POP RCX
	POP RAX
	POP RDI
	*/
	const ByteVector kPopGeneralPurposeRegs {
		0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x41, 0x5B, 0x41,
		0x5A, 0x41, 0x59, 0x41, 0x58, 0x5E, 0x5D, 0x5B, 0x5A, 0x59, 0x58,
		0x5F
	};

	// MOV RCX, RBP
	const ByteVector kMovRCXRBP{ 0x48, 0x89, 0xE9 };

	inline void CallRDI(FunctionBuffer *fb, const uint64_t fn_ptr) {
		/*
		Move func ptr to RDI
		Call RDI
		*/
		ByteVector bytes{
			0x48, 0xBF,
			0, 0, 0, 0, 0, 0, 0, 0,
			0x48, 0x83, 0xEC, 0x50,
			0xFF, 0xD7,
			0x48, 0x83, 0xC4, 0x50
		};
		*reinterpret_cast<uint64_t*>(&bytes.data()[2]) = fn_ptr;
		fb->Write(bytes);
	}

	inline void Jump(FunctionBuffer *fb, const uint64_t addr) {
		// jmp QWORD PTR [rip+addr]
		ByteVector bytes{
			0xFF, 0x25,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0
		};

		*reinterpret_cast<uint64_t*>(&bytes.data()[6]) = addr;
		fb->Write(bytes);
	}

	inline void SubRCX(FunctionBuffer *fb, const int val) {
		ByteVector bytes{ 0x48, 0x81, 0xE9, 0, 0, 0, 0 };
		*reinterpret_cast<uint64_t*>(&bytes.data()[3]) = val;
		fb->Write(bytes);
	}

	std::unordered_map<uint64_t, Hook> hooks_; // Holds information about currently hooked functions


	/// <summary>Read and convert relative jump to address from jump instruction to absolute address</summary>
	/// <param="address">Address of jump instruction</param>
	/// <returns>Absolute address</returns>
	uint64_t ReadJump(const uint64_t address) {
		uint32_t rel_fn_ptr = *reinterpret_cast<uint32_t*>(address + 1);
		return address + rel_fn_ptr + 6;
	}

	bool CreateTrampoline(const uint64_t address, const uint64_t dest_address, const int byte_count)
	{
		if (hooks_.find(address) != hooks_.end()) {
			return false;
		}


		auto &hook = hooks_[address];
		const auto &fb = hook.fb;

		DWORD old_flag;
		uint8_t *fn_bytes = reinterpret_cast<uint8_t*>(address);

		hook.fn_ptr = address;
		hook.orig_bytes.assign(fn_bytes, fn_bytes + byte_count);



		// Allow writing
		if (!VirtualProtect(fn_bytes, byte_count, PAGE_EXECUTE_READWRITE, &old_flag)) {
			return false;
		}


		fb->Write(kPushGeneralPurposeRegs);
		fb->Write(kSaveXMMRegs);

		// Mov RBP to RCX, so registers on stack can be accessed by casting RCX to Registers struct
		fb->Write(kMovRCXRBP);


		CallRDI(fb.get(), dest_address);

		// Restore stack
		fb->Write(kRestoreXMMRegs);
		fb->Write(kPopGeneralPurposeRegs);


		// Copy function bytes that were replaced by the jump to end of the hook
		fb->Write(hook.orig_bytes);

		// Jump back to the original function
		Jump(fb.get(), reinterpret_cast<uint64_t>(fn_bytes) + byte_count);

		if (!fb->SetAsExecutable()) {
			return false;
		}

		// Set jump to hook
		uint8_t push_jump[] = {
			0x57, 0x48, 0xBF,
			0, 0, 0, 0, 0, 0, 0, 0,
			0x57, 0xC3
		};

		*reinterpret_cast<uint64_t*>(&push_jump[3]) = reinterpret_cast<uint64_t>(fb->data());
		memcpy(fn_bytes, push_jump, sizeof(push_jump));

		// NOP padding for alignment
		std::memset(&fn_bytes[sizeof(push_jump)], 0x90, byte_count - sizeof(push_jump));

		// Restore protection
		if (!VirtualProtect(fn_bytes, byte_count, old_flag, &old_flag)) {
			return false;
		}

		return true;
	}

	/// <summary>Overwrite function pointer in virtual table</summary>
	/// <param="vtable_fn_ptr">Location of virtual table function pointer</param>
	/// <param="fn_ptr">Value written</param>
	bool OverwriteVTableFunction(const uint64_t vtable_fn_ptr, const uint64_t fn_ptr)
	{
		if (hooks_.find(vtable_fn_ptr) != hooks_.end()) {
			return false;
		}


		auto &hook = hooks_[vtable_fn_ptr];

		DWORD old_flag;
		uint8_t *bytes = reinterpret_cast<uint8_t*>(vtable_fn_ptr);

		hook.fn_ptr = vtable_fn_ptr;
		hook.orig_bytes.assign(bytes, bytes + sizeof(uint64_t));

		// Allow writing
		if (!VirtualProtect(bytes, sizeof(uint64_t), PAGE_EXECUTE_READWRITE, &old_flag)) {
			return false;
		}

		*reinterpret_cast<uint64_t*>(bytes) = fn_ptr;

		// Restore protection
		if (!VirtualProtect(bytes, sizeof(uint64_t), old_flag, &old_flag)) {
			return false;
		}

		return true;
	}


	template <typename T = void(Registers)>
	bool InlineHook(const uint64_t address, const std::function<void(Registers)> fn, const int byte_count) {
#ifndef _WIN64
#error "Only supports x64 Windows"
#endif // !_WIN64

		assert(byte_count >= 13);

		if (fn.target<T*>() == nullptr) {
			return false;
		} 

		if (hooks_.find(address) != hooks_.end()) {
			return false;
		}

		uint64_t fn_ptr = reinterpret_cast<uint64_t>(*fn.target<T*>());

		return CreateTrampoline(address, fn_ptr, byte_count);
	}

	template <typename T>
	bool VTableHook(const uint64_t address, const std::function<T> fn) {
#ifndef _WIN64
#error "Only supports x64 Windows"
#endif // !_WIN64

		if (fn.target<T*>() == nullptr) {
			return false;
		}

		if (hooks_.find(address) != hooks_.end()) {
			return false;
		}

		uint64_t fn_ptr = reinterpret_cast<uint64_t>(*fn.target<T*>());
		return OverwriteVTableFunction(address, fn_ptr);
	}


	/*
	Do not call inside the hooked function (can crash due to dellocation of FunctionBuffer)
	*/
	bool Unhook(const uint64_t address) {
		if (hooks_.find(address) == hooks_.end()) {
			return false;
		}

		const auto &hook = hooks_[address];

		DWORD old_flag;


		// Allow writing
		if (!VirtualProtect(reinterpret_cast<void*>(address), hook.orig_bytes.size(), PAGE_EXECUTE_READWRITE, &old_flag)) {
			return false;
		}

		// Restore original bytes
		memcpy(reinterpret_cast<void*>(hook.fn_ptr), hook.orig_bytes.data(), hook.orig_bytes.size());

		// Restore protection
		if (!VirtualProtect(reinterpret_cast<void*>(address), hook.orig_bytes.size(), old_flag, &old_flag)) {
			return false;
		}


		// Remove and deallocate function buffer
		hooks_.erase(address);
		return true;
	}

	/*
	Do not call inside a hooked function (can crash due to dellocation of FunctionBuffer)
	*/
	void Unhook() {
		while (hooks_.size() > 0) {
			Unhook(hooks_.begin()->first);
		}
	}


}

