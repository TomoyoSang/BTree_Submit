#include "utility.hpp"
#include <functional>
#include <cstddef>
#include "exception.hpp"
#include <cstdio>
namespace sjtu {
	//文件指针
	FILE* bp_file = NULL;
	//bptree地址
	constexpr char _address[100] = "Tomoyo_no_bptree";

	template <class Key, class Value, class Compare = std::less<Key> >
	class BTree {
	private:
		// Your private members go here
//块的开头部分
				//这是block的头，存储该块的基本信息，如是否为叶子、块内有多少数据等，，index和leaf共用这一种head
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

		//主体部分：索引或数据
				//这是index的构成成分
		struct Block_index_node
		{
			int _child = 0;
			Key _key;
		};
		//leaf的构成成分就是pair


		//这是BPtree的杂项数据信息：		
		static const int BLOCK_SIZE = 4096;
		static const int HEAD_SIZE = sizeof(Block_Head);
		static const int index_node_len = (BLOCK_SIZE - HEAD_SIZE) / sizeof(Block_index_node);
		static const int leaf_node_len = (BLOCK_SIZE - HEAD_SIZE) / (sizeof(Key) + sizeof(Value));


//文件部分
		//文件头
		class BPLUSFILES_INFO
		{
		public:
			int block_num = 1;
			int data_num = 0;
			int data_head = 0;
			int data_rear = 0;
			int root_pos = 0;
		};


		//文件头声明
		BPLUSFILES_INFO bpfile_info;



		//这是index的整体成分
		class Block_index
		{
		public:
			Block_index_node  inodes[index_node_len];
		};
		//这是leaf的整体部分
		class Block_leaf
		{
		public:
			pair<Key, Value>  inodes[leaf_node_len];
		};

		//内置函数部分：
		//这是bptree的关键，写不好要被助教处理的！！！怖いです
		//頑張ってね！！！
				//读取块：先将指针移动至目标部分，再进行固定长度读取
		template <class T>
		static void readin(T info_container, int info_container_size, int pos)
		{
			fseek(bp_file, info_container_size * pos, SEEK_SET);
			fread(info_container, info_container_size, 1, bp_file);
		}

		//输入块：先将指针移动到指定位置，再输入，并用fflush保证输入全部进入文件，防止文件关闭而导致
		template <class T>
		static void writein(T info_container, int info_container_size, int pos)
		{
			fseek(bp_file, long(info_container_size * pos), SEEK_SET);
			fwrite(info_container, info_container_size, 1, bp_file);
			fflush(bp_file);
		}

		//整块更新树的信息：从内存到外存
		void renew_info()
		{
			fseek(bp_file, 0, SEEK_SET);
			char info_container[BLOCK_SIZE] = { 0 };
			memcpy(info_container, &bpfile_info, sizeof(bpfile_info));
			writein(info_container, BLOCK_SIZE, 0);
		}

		//申请文件内空间，并把申请的可用地址用0 memset
		int apply_memory()
		{
			++bpfile_info.block_num;
			renew_info();
			char info_container[BLOCK_SIZE] = { 0 };
			writein(info_container, BLOCK_SIZE, bpfile_info.block_num - 1);
			return bpfile_info.block_num - 1;
		}

		//申请head和index，并进行初始化，返回该地址
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

		//申请head和leaf，并进行初始化，返回该地址
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

		//插入新索引：
		//在不超大小的情况下，将origin位置的key以及之后的都往后移一格，再把new_pos的_child和_key分别放到origin及它之后的位置 
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

		//把在内存创建好的head和index/leaf放入文件里
		template <class T>
		static void renew_block(Block_Head* _info, T* _data, int _pos)
		{
			char info_container[BLOCK_SIZE] = { 0 };
			memcpy(info_container, _info, sizeof(Block_Head));
			memcpy(info_container + HEAD_SIZE, _data, sizeof(T));
			writein(info_container, BLOCK_SIZE, _pos);
		}

		//把_pos位置上的信息读入info_container,再分别转移到head和data里
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


		void change_index(int l_parent, int l_child, const Key& new_key)
		{

			Block_Head parent_info;
			Block_index parent_data;
			copy_block(&parent_info, &parent_data, l_parent);
			if (parent_data.inodes[parent_info.data_num - 1]._child == l_child) {
				change_index(parent_info._parent, l_parent, new_key);
				return;
			}
			for (int cur_pos = parent_info.data_num - 2;; --cur_pos) {
				if (parent_data.inodes[cur_pos]._child == l_child) {
					parent_data.inodes[cur_pos]._key = new_key;
					break;
				}
			}
			renew_block(&parent_info, &parent_data, l_parent);
		}


		void merge_normal(Block_Head& l_info, Block_index& l_data, Block_Head& r_info, Block_index& r_data)
		{
			for (int p = l_info.data_num, i = 0; i < r_info.data_num; ++p, ++i)
			{
				l_data.inodes[p] = r_data.inodes[i];

			}
			l_data.inodes[l_info.data_num - 1]._key = adjust_normal(r_info._parent, r_info._pos);
			l_info.data_num += r_info.data_num;
			renew_block(&l_info, &l_data, l_info._pos);
		}


		void balance_normal(Block_Head& info, Block_index& index_data)
		{
			if (info.data_num >= index_node_len / 2) {
				renew_block(&info, &index_data, info._pos);
				return;
			}

			if (info._pos == bpfile_info.root_pos && info.data_num <= 1) {
				bpfile_info.root_pos = index_data.inodes[0]._child;
				renew_info();
				return;
			}
			else if (info._pos == bpfile_info.root_pos) {
				renew_block(&info, &index_data, info._pos);
				return;
			}

			Block_Head parent_info, brother_info;
			Block_index parent_data, brother_data;
			copy_block(&parent_info, &parent_data, info._parent);
			int value_pos;
			for (value_pos = 0; parent_data.inodes[value_pos]._child != info._pos; ++value_pos);
			if (value_pos > 0) {
				copy_block(&brother_info, &brother_data, parent_data.inodes[value_pos - 1]._child);
				brother_info._parent = info._parent;
				if (brother_info.data_num > index_node_len / 2) {
					for (int p = info.data_num; p > 0; --p) {
						index_data.inodes[p] = index_data.inodes[p - 1];
					}
					index_data.inodes[0]._child = brother_data.inodes[brother_info.data_num - 1]._child;
					index_data.inodes[0]._key = parent_data.inodes[value_pos - 1]._key;
					parent_data.inodes[value_pos - 1]._key = brother_data.inodes[brother_info.data_num - 2]._key;
					--brother_info.data_num;
					++info.data_num;
					renew_block(&brother_info, &brother_data, brother_info._pos);
					renew_block(&info, &index_data, info._pos);
					renew_block(&parent_info, &parent_data, parent_info._pos);
					return;
				}
				else
				{
					merge_normal(brother_info, brother_data, info, index_data);
					return;
				}
			}
			if (value_pos < parent_info.data_num - 1) {
				copy_block(&brother_info, &brother_data, parent_data.inodes[value_pos + 1]._child);
				brother_info._parent = info._parent;
				if (brother_info.data_num > index_node_len / 2)
				{
					index_data.inodes[info.data_num]._child = brother_data.inodes[0]._child;
					index_data.inodes[info.data_num - 1]._key = parent_data.inodes[value_pos]._key;
					parent_data.inodes[value_pos]._key = brother_data.inodes[0]._key;
					for (int p = 1; p < brother_info.data_num; ++p)
					{
						brother_data.inodes[p - 1] = brother_data.inodes[p];
					}
					--brother_info.data_num;
					++info.data_num;
					renew_block(&brother_info, &brother_data, brother_info._pos);
					renew_block(&info, &index_data, info._pos);
					renew_block(&parent_info, &parent_data, parent_info._pos);
					return;
				}
				else
				{
					merge_normal(info, index_data, brother_info, brother_data);
					return;
				}
			}
		}


		Key adjust_normal(int pos, int removed_child)
		{
			Block_Head info;
			Block_index normal_data;
			copy_block(&info, &normal_data, pos);
			int cur_pos;
			for (cur_pos = 0; normal_data.inodes[cur_pos]._child != removed_child; ++cur_pos);
			Key ans = normal_data.inodes[cur_pos - 1]._key;
			normal_data.inodes[cur_pos - 1]._key = normal_data.inodes[cur_pos]._key;
			for (; cur_pos < info.data_num - 1; ++cur_pos)
			{
				normal_data.inodes[cur_pos] = normal_data.inodes[cur_pos + 1];
			}
			--info.data_num;
			balance_normal(info, normal_data);
			return ans;
		}


		void merge_leaf(Block_Head& l_info, Block_leaf& l_data, Block_Head& r_info, Block_leaf& r_data)
		{
			for (int p = l_info.data_num, i = 0; i < r_info.data_num; ++p, ++i) {
				l_data.inodes[p].first = r_data.inodes[i].first;
				l_data.inodes[p].second = r_data.inodes[i].second;
			}
			l_info.data_num += r_info.data_num;
			adjust_normal(r_info._parent, r_info._pos);

			l_info._right = r_info._right;
			Block_Head temp_info;
			Block_leaf temp_data;
			copy_block(&temp_info, &temp_data, r_info._right);
			temp_info._left = l_info._pos;
			renew_block(&temp_info, &temp_data, temp_info._pos);
			renew_block(&l_info, &l_data, l_info._pos);
		}


		void balance_leaf(Block_Head& leaf_info, Block_leaf& leaf_data)
		{
			if (leaf_info.data_num >= leaf_node_len / 2) {
				renew_block(&leaf_info, &leaf_data, leaf_info._pos);
				return;
			}
			else if (leaf_info._pos == bpfile_info.root_pos) {
				if (leaf_info.data_num == 0) {
					Block_Head temp_info;
					Block_leaf temp_data;
					copy_block(&temp_info, &temp_data, bpfile_info.data_head);
					temp_info._right = bpfile_info.data_rear;
					renew_block(&temp_info, &temp_data, bpfile_info.data_head);
					copy_block(&temp_info, &temp_data, bpfile_info.data_rear);
					temp_info._left = bpfile_info.data_head;
					renew_block(&temp_info, &temp_data, bpfile_info.data_rear);
					return;
				}
				renew_block(&leaf_info, &leaf_data, leaf_info._pos);
				return;
			}

			Block_Head brother_info, parent_info;
			Block_leaf brother_data;
			Block_index parent_data;

			copy_block(&parent_info, &parent_data, leaf_info._parent);
			int node_pos = 0;
			for (; node_pos < parent_info.data_num; ++node_pos) {
				if (parent_data.inodes[node_pos]._child == leaf_info._pos)
					break;
			}


			if (node_pos > 0) {
				copy_block(&brother_info, &brother_data, leaf_info._left);
				brother_info._parent = leaf_info._parent;
				if (brother_info.data_num > leaf_node_len / 2) {
					for (int p = leaf_info.data_num; p > 0; --p) {
						leaf_data.inodes[p].first = leaf_data.inodes[p - 1].first;
						leaf_data.inodes[p].second = leaf_data.inodes[p - 1].second;
					}
					leaf_data.inodes[0].first = brother_data.inodes[brother_info.data_num - 1].first;
					leaf_data.inodes[0].second = brother_data.inodes[brother_info.data_num - 1].second;
					--brother_info.data_num;
					++leaf_info.data_num;
					change_index(brother_info._parent, brother_info._pos, leaf_data.inodes[0].first);
					renew_block(&brother_info, &brother_data, brother_info._pos);
					renew_block(&leaf_info, &leaf_data, leaf_info._pos);
					return;
				}
				else {
					merge_leaf(brother_info, brother_data, leaf_info, leaf_data);
					//renew_block(&brother_info, &brother_data, brother_info._pos);
					return;
				}
			}

			if (node_pos < parent_info.data_num - 1) {
				copy_block(&brother_info, &brother_data, leaf_info._right);
				brother_info._parent = leaf_info._parent;
				if (brother_info.data_num > leaf_node_len / 2) {
					leaf_data.inodes[leaf_info.data_num].first = brother_data.inodes[0].first;
					leaf_data.inodes[leaf_info.data_num].second = brother_data.inodes[0].second;
					for (int p = 1; p < brother_info.data_num; ++p) {
						brother_data.inodes[p - 1].first = brother_data.inodes[p].first;
						brother_data.inodes[p - 1].second = brother_data.inodes[p].second;
					}
					++leaf_info.data_num;
					--brother_info.data_num;
					change_index(leaf_info._parent, leaf_info._pos, brother_data.inodes[0].first);
					renew_block(&leaf_info, &leaf_data, leaf_info._pos);
					renew_block(&brother_info, &brother_data, brother_info._pos);
					return;
				}
				else {
					merge_leaf(leaf_info, leaf_data, brother_info, brother_data);
					//renew_block(&brother_info, &brother_data, brother_info._pos);
					return;
				}
			}
		}


		void check_file() {
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
			char buff[BLOCK_SIZE] = { 0 };
			readin(buff, BLOCK_SIZE, 0);
			memcpy(&bpfile_info, buff, sizeof(bpfile_info));
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

		pair<iterator, OperationResult> insert(const Key& key, const Value& value)
		{
			check_file();
			if (empty())
			{
				auto root_pos = apply_block_leaf(0, bpfile_info.data_head, bpfile_info.data_rear);

				Block_Head temp_info;
				Block_leaf temp_data;

				//链接head
				copy_block(&temp_info, &temp_data, bpfile_info.data_head);
				temp_info._right = root_pos;
				renew_block(&temp_info, &temp_data, bpfile_info.data_head);

				//链接rear
				copy_block(&temp_info, &temp_data, bpfile_info.data_rear);
				temp_info._left = root_pos;
				renew_block(&temp_info, &temp_data, bpfile_info.data_rear);

				//插入pair
				copy_block(&temp_info, &temp_data, root_pos);
				++temp_info.data_num;
				temp_data.inodes[0].first = key;
				temp_data.inodes[0].second = value;
				renew_block(&temp_info, &temp_data, root_pos);


				//更新bpfile_info
				++bpfile_info.data_num;
				bpfile_info.root_pos = root_pos;
				renew_info();

				pair<iterator, OperationResult> result(begin(), Success);
				return result;
			}


			//查找正确的节点位置
			char buff[BLOCK_SIZE] = { 0 };
			int cur_pos = bpfile_info.root_pos, cur_parent = 0;
			while (true) {
				readin(buff, BLOCK_SIZE, cur_pos);
				Block_Head tmp;
				memcpy(&tmp, buff, sizeof(tmp));

				//判断父亲是否更新
				if (cur_parent != tmp._parent)
				{
					tmp._parent = cur_parent;
					memcpy(buff, &tmp, sizeof(tmp));
					writein(buff, BLOCK_SIZE, cur_pos);
				}
				if (tmp.isleaf)
				{
					break;
				}

				Block_index normal_data;
				memcpy(&normal_data, buff + HEAD_SIZE, sizeof(normal_data));
				int child_pos = tmp.data_num - 1;
				for (; child_pos > 0; --child_pos) {
					if (!(normal_data.inodes[child_pos - 1]._key > key)) {
						break;
					}
				}
				cur_parent = cur_pos;
				cur_pos = normal_data.inodes[child_pos]._child;

			}

			Block_Head info;
			memcpy(&info, buff, sizeof(info));
			Block_leaf leaf_data;
			memcpy(&leaf_data, buff + HEAD_SIZE, sizeof(leaf_data));
			for (int value_pos = 0;; ++value_pos) {
				if (value_pos < info.data_num && (!(leaf_data.inodes[value_pos].first<key || leaf_data.inodes[value_pos].first>key))) {
					//throw runtime_error();
					return pair<iterator, OperationResult>(end(), Fail);
				}
				if (value_pos >= info.data_num || leaf_data.inodes[value_pos].first > key) {
					//在此结点之前插入
					if (info.data_num >= leaf_node_len) {
						auto cur_key = split_leaf(cur_pos, info, leaf_data);
						if (key > cur_key) {
							cur_pos = info._right;
							value_pos -= info.data_num;
							copy_block(&info, &leaf_data, cur_pos);
						}
					}

					for (int p = info.data_num - 1; p >= value_pos; --p) {
						leaf_data.inodes[p + 1].first = leaf_data.inodes[p].first;
						leaf_data.inodes[p + 1].second = leaf_data.inodes[p].second;
						if (p == value_pos)
							break;
					}
					leaf_data.inodes[value_pos].first = key;
					leaf_data.inodes[value_pos].second = value;
					++info.data_num;
					renew_block(&info, &leaf_data, cur_pos);
					iterator ans;
					//修改树的基本参数
					++bpfile_info.data_num;
					renew_info();
					pair<iterator, OperationResult> to_return(ans, Success);
					return to_return;
				}
			}
			return pair<iterator, OperationResult>(end(), Fail);
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
				//找到叶子节点
				if (tmp.isleaf)
				{
					break;
				}

				Block_index index_data;
				memcpy(&index_data, info_container + HEAD_SIZE, sizeof(index_data));
				//倒着找，直到找到比其小的key则其后面就是目标节点

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

}  // namespace sjtu
