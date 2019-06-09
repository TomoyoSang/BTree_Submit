#include "utility.hpp"
#include <functional>
#include <cstddef>
#include "exception.hpp"
#include <cstdio>
namespace sjtu {
	//�ļ�ָ��
	FILE* bp_file = NULL;
	//bptree��ַ
	constexpr char _address[100] = "Tomoyo_no_bptree";

	template <class Key, class Value, class Compare = std::less<Key> >
	class BTree {
	private:
		// Your private members go here
//����BPtree����Ҫ��Ϣ��		
		const int BLOCK_SIZE = 4096;
		const int HEAD_SIZE = sizeof(Block_Head);
		const int index_node_len = (BLOCK_SIZE - HEAD_SIZE) / sizeof(Block_index_node) - 1;
		const int leaf_node_len = (BLOCK_SIZE - HEAD_SIZE) / (sizeof(Key) + sizeof(Value)) - 1;

//�ļ�����
		//�ļ�ͷ
		class BPLUSFILES_INFO
		{
		public:
			int block_num = 1;
			int data_num = 0;
			int data_head = 0;
			int data_rear = 0;
			int root_pos = 0;
		};


        //�ļ�ͷ����
		BPLUSFILES_INFO bpfile_info;
		
//��Ŀ�ͷ����
        //����block��ͷ���洢�ÿ�Ļ�����Ϣ�����Ƿ�ΪҶ�ӡ������ж������ݵȣ���index��leaf������һ��head
		class Block_Head
		{
		public:
			bool isleaf = 0;
			int data_num = 0;
			int _pos = 0;
			int _parent = 0;
			int _left = 0;
			int _right = 0;
		};

//���岿�֣�����������
		//����index�Ĺ��ɳɷ�
		struct Block_index_node
		{
			int _child = 0;
			Key _key;
		};
		//leaf�Ĺ��ɳɷ־���pair


		//����index������ɷ�
		class Block_index 
		{
		public:
			Block_index_node  inodes[index_node_len];
		};
		//����leaf�����岿��
		class Block_leaf 
		{
		public:
			pair<Key, Value>  inodes[leaf_node_len];
		};





//���ú������֣�
//����bptree�Ĺؼ���д����Ҫ�����̴���ģ����������Ǥ�
//�B���äƤͣ�����
        //��ȡ�飺�Ƚ�ָ���ƶ���Ŀ�겿�֣��ٽ��й̶����ȶ�ȡ
		template <class T>
		static void readin(T info_container, int info_container_size, int pos)
		{
			fseek(bp_file,info_container_size * pos, SEEK_SET);
			fread(info_container, info_container_size, 1, bp_file);
		}

		//����飺�Ƚ�ָ���ƶ���ָ��λ�ã������룬����fflush��֤����ȫ�������ļ�����ֹ�ļ��رն�����
		template <class T>
		static void writein(T info_container, int info_container_size, int pos)
		{
			fseek(bp_file, long(info_container_size * pos), SEEK_SET);
			fwrite(info_container, info_container_size, 1, bp_file);
			fflush(bp_file);
		}

		//�������������Ϣ�����ڴ浽���
		void renew_info()
		{
			fseek(bp_file, 0, SEEK_SET);
			char info_container[BLOCK_SIZE] = { 0 };
			memcpy(info_container, &bpfile_info, sizeof(bpfile_info));
			writein(info_container, BLOCK_SIZE, 0);
		}

		//�����ļ��ڿռ䣬��������Ŀ��õ�ַ��0 memset
		int apply_memory()
		{
			++bpfile_info.block_num;
			renew_info();
			char info_container[BLOCK_SIZE] = { 0 };
			writein(info_container, BLOCK_SIZE, bpfile_info.block_num - 1);
			return bpfile_info.block_num - 1;
		}

		//����head��index�������г�ʼ�������ظõ�ַ
		int apply_block_index(int _parent)
		{
			auto node_pos = apply_memory();
			Block_Head tmp;
			Block_index index_data;

			tmp.isleaf = false;
			tmp._parent = _parent;
			tmp._pos = node_pos;
			tmp.data_num = 0;

			renew_block(&tmp, &index_data, node_pos);
			return node_pos;
		}

		//����head��leaf�������г�ʼ�������ظõ�ַ
		int apply_block_leaf(int _parent, int _left, int _right)
		{
			auto node_pos = apply_memory();
			Block_Head tmp;
			Block_leaf leaf_data;

			tmp.isleaf = true;
			tmp._parent = _parent;
			tmp._pos = node_pos;
			tmp._left = _left;
			tmp._right = _right;
			tmp.data_num = 0;

			renew_block(&tmp, &leaf_data, node_pos);
			return node_pos;
		}

		//������������
		//�ڲ�����С������£���originλ�õ�key�Լ�֮��Ķ�������һ���ٰ�new_pos��_child��_key�ֱ�ŵ�origin����֮���λ�� 
		void insert_indexnode(Block_Head& par_info, Block_index& par_data, int the_past, int new_pos, const Key& new_index)
		{
			++par_info.data_num;
			auto p = par_info.data_num - 2;
			for (; par_data.inodes[p]._child != the_past; --p)
			{
				par_data.inodes[p + 1] = par_data.inodes[p];
			}

			par_data.inodes[p + 1]._key = par_data.inodes[p]._key;
			par_data.inodes[p]._key = new_index;
			par_data.inodes[p + 1]._child = new_pos;
		}

		//�����ڴ洴���õ�head��index/leaf�����ļ���
		template <class T>
		static void renew_block(Block_Head* _info, T* _data, int _pos)
		{
			char info_container[BLOCK_SIZE] = { 0 };
			memcpy(info_container, _info, sizeof(Block_Head));
			memcpy(info_container + HEAD_SIZE, _data, sizeof(T));
			writein(info_container, BLOCK_SIZE, _pos);
		}

		//��_posλ���ϵ���Ϣ����info_container,�ٷֱ�ת�Ƶ�head��data��
		template <class T>
		static void copy_block(Block_Head* _info, T* _data, int _pos)
		{
			char info_container[BLOCK_SIZE] = { 0 };
			readin(info_container, BLOCK_SIZE, _pos);
			memcpy(_info, info_container, sizeof(Block_Head));
			memcpy(_data, info_container + HEAD_SIZE, sizeof(T));
		}

		Key split_leaf(int pos, Block_Head& the_past_info, Block_leaf& the_past_data)
		{
			int parent_pos;
			Block_Head parent_info;
			Block_index parent_data;

			if (pos == bpfile_info.root_pos)
			{
				auto root_pos = apply_block_index(0);
				bpfile_info.root_pos = root_pos;
				renew_info();
				copy_block(&parent_info, &parent_data, root_pos);
				the_past_info._parent = root_pos;
				++parent_info.data_num;
				parent_data.inodes[0]._child = pos;
				parent_pos = root_pos;
			}
			else
			{
				copy_block(&parent_info, &parent_data, the_past_info._parent);
				parent_pos = parent_info._pos;
			}

			if (ifparent(the_past_info)) 
			{
				parent_pos = the_past_info._parent;
				copy_block(&parent_info, &parent_data, parent_pos);
			}

			
			auto new_pos = apply_block_leaf(parent_pos, pos, the_past_info._right);
			Block_Head new_info;
			Block_leaf new_data;
			copy_block(&new_info, &new_data, new_pos);

			
			auto temp_pos = the_past_info._right;
			Block_Head temp_info;
			Block_leaf temp_data;
			copy_block(&temp_info, &temp_data, temp_pos);
			temp_info._left = new_pos;
			renew_block(&temp_info, &temp_data, temp_pos);
			
			the_past_info._right = new_pos;

			int mid_pos = the_past_info.data_num >> 1;
			for (int p = mid_pos, i = 0; p < the_past_info.data_num; ++p, ++i) {
				new_data.inodes[i].first = the_past_data.inodes[p].first;
				new_data.inodes[i].second = the_past_data.inodes[p].second;
				++new_info.data_num;
			}
			the_past_info.data_num = mid_pos;
			
			insert_indexnode(parent_info, parent_data, pos, new_pos, the_past_data.inodes[mid_pos].first);

			
			renew_block(&the_past_info, &the_past_data, pos);
			renew_block(&new_info, &new_data, new_pos);
			renew_block(&parent_info, &parent_data, parent_pos);

			return new_data.inodes[0].first;
		}

		bool ifparent(Block_Head& child_info)
		{
			int parent_pos, the_past_pos = child_info._parent;
			Block_Head parent_info, the_past_info;
			Block_index parent_data, the_past_data;
			copy_block(&the_past_info, &the_past_data, the_past_pos);
			if (the_past_info.data_num < index_node_len) return false;

			if (the_past_pos == bpfile_info.root_pos) 
			{
				
				auto root_pos = apply_block_index(0);
				bpfile_info.root_pos = root_pos;
				renew_info();
				copy_block(&parent_info, &parent_data, root_pos);
				the_past_info._parent = root_pos;
				++parent_info.data_num;
				parent_data.inodes[0]._child = the_past_pos;
				parent_pos = root_pos;
			}
			else
			{
				copy_block(&parent_info, &parent_data, the_past_info._parent);
				parent_pos = parent_info._pos;
			}
			if (ifparent(the_past_info))
			{
				parent_pos = the_past_info._parent;
				copy_block(&parent_info, &parent_data, parent_pos);
			}
			
			auto new_pos = apply_block_index(parent_pos);
			Block_Head new_info;
			Block_index new_data;
			copy_block(&new_info, &new_data, new_pos);

			int mid_pos = the_past_info.data_num / 2;
			for (int p = mid_pos + 1, i = 0; p < the_past_info.data_num; ++p, ++i) 
			{
				if (the_past_data.inodes[p]._child == child_info._pos) 
				{
					child_info._parent = new_pos;
				}
				std::swap(new_data.inodes[i], the_past_data.inodes[p]);
				++new_info.data_num;
			}
			the_past_info.data_num = mid_pos + 1;
			insert_indexnode(parent_info, parent_data, the_past_pos, new_pos, the_past_data.inodes[mid_pos]._key);

			renew_block(&the_past_info, &the_past_data, the_past_pos);
			renew_block(&new_info, &new_data, new_pos);
			renew_block(&parent_info, &parent_data, parent_pos);
			return true;
		}

	public:
		typedef pair<const Key, Value> value_type;



		class const_iterator;
		class iterator {
		private:
			// Your private members go here
		public:
			bool modify(const Value& value) {

			}
			iterator() {
				// TODO Default Constructor
			}
			iterator(const iterator& other) {
				// TODO Copy Constructor
			}
			// Return a new iterator which points to the n-next elements
			iterator operator++(int) {
				// Todo iterator++
			}
			iterator& operator++() {
				// Todo ++iterator
			}
			iterator operator--(int) {
				// Todo iterator--
			}
			iterator& operator--() {
				// Todo --iterator
			}
			// Overloaded of operator '==' and '!='
			// Check whether the iterators are same
			bool operator==(const iterator& rhs) const {
				// Todo operator ==
			}
			bool operator==(const const_iterator& rhs) const {
				// Todo operator ==
			}
			bool operator!=(const iterator& rhs) const {
				// Todo operator !=
			}
			bool operator!=(const const_iterator& rhs) const {
				// Todo operator !=
			}
		};
		class const_iterator {
			// it should has similar member method as iterator.
			//  and it should be able to construct from an iterator.
		private:
			// Your private members go here
		public:
			const_iterator() {
				// TODO
			}
			const_iterator(const const_iterator& other) {
				// TODO
			}
			const_iterator(const iterator& other) {
				// TODO
			}
			// And other methods in iterator, please fill by yourself.
		};





		// Default Constructor and Copy Constructor
		BTree() 
		{
			bp_file = fopen(_address, "rb+");
			if (!bp_file) {
				
				bp_file = fopen(_address, "wb+");
				renew_info();

				auto node_head = bpfile_info.block_num,
					node_rear = bpfile_info.block_num + 1;

				bpfile_info.data_head = node_head;
				bpfile_info.data_rear = node_rear;

				apply_block_leaf(0, 0, node_rear);
				apply_block_leaf(0, node_head, 0);

				return;
			}
			char info_container[BLOCK_SIZE] = { 0 };
			readin(info_container, BLOCK_SIZE, 0);
			memcpy(&bpfile_info, info_container, sizeof(bpfile_info));
			
		}

		BTree(const BTree& other) 
		{
			bp_file = fopen(_address, "rb+");
			bpfile_info.data_rear = other.bpfile_info.data_rear;
			bpfile_info.root_pos = other.bpfile_info.root_pos;
			bpfile_info.data_num = other.bpfile_info.data_num;
		
			bpfile_info.block_num = other.bpfile_info.block_num;
			bpfile_info.data_head = other.bpfile_info.data_head;
		}

		BTree& operator=(const BTree& other) 
		{
			bp_file = fopen(_address, "rb+");
			bpfile_info.block_num = other.bpfile_info.block_num;
			bpfile_info.data_head = other.bpfile_info.data_head;
			bpfile_info.data_rear = other.bpfile_info.data_rear;
			bpfile_info.root_pos = other.bpfile_info.root_pos;
			bpfile_info.data_num = other.bpfile_info.data_num;
			return *this;
		}
		~BTree() {

			fclose(bp_file);
		}

		// Insert: Insert certain Key-Value into the database
	    // Return a pair, the first of the pair is the iterator point to the new
	    // element, the second of the pair is Success if it is successfully inserted
		pair<iterator, OperationResult> insert(const Key& key, const Value& value) {

		}
			

		// Erase: Erase the Key-Value
		// Return Success if it is successfully erased
		// Return Fail if the key doesn't exist in the database
		OperationResult erase(const Key& key) {
			// TODO erase function
			return Fail;  // If you can't finish erase part, just remaining here.
		}


		// Return a iterator to the beginning
		iterator begin() {}
		const_iterator cbegin() const {}
		// Return a iterator to the end(the next element after the last)
		iterator end() {}
		const_iterator cend() const {}


		bool empty() const 
		{
			if (!bp_file)return true;

			return bpfile_info.data_num == 0;
		}


		int size() const 
		{
			if (!bp_file) return 0;

			return bpfile_info.data_num;
		}


		void clear()
		{
			bp_file = NULL;
			return;
		}
	



		Value at(const Key& key) {
			if (empty()) 
			{
				throw container_is_empty();
			}
			char info_container[BLOCK_SIZE] = { 0 };
			int cur_pos = bpfile_info.root_pos, cur_parent = 0;
			while (true)
			{
				readin(info_container, BLOCK_SIZE, cur_pos);
				Block_Head tmp;
				memcpy(&tmp, info_container, sizeof(tmp));

				if (cur_parent != tmp._parent) {
					tmp._parent = cur_parent;
					memcpy(info_container, &tmp, sizeof(tmp));
					writein(info_container, BLOCK_SIZE, cur_pos);
				}
				//�ҵ�Ҷ�ӽڵ�
				if (tmp.isleaf) 
				{
					break;
				}

				Block_index index_data;
				memcpy(&index_data, info_container + HEAD_SIZE, sizeof(index_data));
				//�����ң�ֱ���ҵ�����С��key����������Ŀ��ڵ�

				int child_pos = tmp.data_num - 1;
				for (; child_pos > 0; --child_pos) 
				{
					if (!(index_data.inodes[child_pos - 1]._key > key))
					{
						break;
					}
				}
				cur_pos = index_data.inodes[child_pos]._child;

			}
			Block_Head head_info;
			memcpy(&head_info, info_container, sizeof(head_info));
			Block_leaf leaf_data;
			memcpy(&leaf_data, info_container + HEAD_SIZE, sizeof(leaf_data));
			for (int value_pos = 0;; ++value_pos) {
				if (value_pos < head_info.data_num && (!(leaf_data.inodes[value_pos].first<key || leaf_data.inodes[value_pos].first>key))) 
				{
					return leaf_data.inodes[value_pos].second;
				}
				if (value_pos >= head_info.data_num || leaf_data.inodes[value_pos].first > key)
				{
					throw index_out_of_bound();
				}
			}
		}
		/**
		 * Returns the number of elements with key
		 *   that compares equivalent to the specified argument,
		 * The default method of check the equivalence is !(a < b || b > a)
		 */
		size_t count(const Key& key) const
		{
			if (find(key) == cend())
			return 0;
			return 1;
		}
		/**
		 * Finds an element with key equivalent to key.
		 * key value of the element to search for.
		 * Iterator to an element with key equivalent to key.
		 *   If no such element is found, past-the-end (see end()) iterator is
		 * returned.
		 */
		iterator find(const Key& key) {}
		const_iterator find(const Key& key) const {}
	};

	template <typename Key, typename Value, typename Compare> FILE* BTree<Key, Value, Compare>::bp_file = NULL;
}  // namespace sjtu