#include <nano/lib/threading.hpp>
#include <nano/secure/store.hpp>

nano::representative_visitor::representative_visitor (nano::transaction const & transaction_a, nano::store & store_a) :
	transaction (transaction_a),
	store (store_a),
	result (0)
{
}

void nano::representative_visitor::compute (nano::block_hash const & hash_a)
{
	current = hash_a;
	while (result.is_zero ())
	{
		auto block (store.block.get (transaction, current));
		debug_assert (block != nullptr);
		block->visit (*this);
	}
}

void nano::representative_visitor::send_block (nano::send_block const & block_a)
{
	current = block_a.previous ();
}

void nano::representative_visitor::receive_block (nano::receive_block const & block_a)
{
	current = block_a.previous ();
}

void nano::representative_visitor::open_block (nano::open_block const & block_a)
{
	result = block_a.hash ();
}

void nano::representative_visitor::change_block (nano::change_block const & block_a)
{
	result = block_a.hash ();
}

void nano::representative_visitor::state_block (nano::state_block const & block_a)
{
	result = block_a.hash ();
}

nano::read_transaction::read_transaction (std::unique_ptr<nano::read_transaction_impl> read_transaction_impl) :
	impl (std::move (read_transaction_impl))
{
}

void * nano::read_transaction::get_handle () const
{
	return impl->get_handle ();
}

void nano::read_transaction::reset () const
{
	impl->reset ();
}

void nano::read_transaction::renew () const
{
	impl->renew ();
}

void nano::read_transaction::refresh () const
{
	reset ();
	renew ();
}

nano::write_transaction::write_transaction (std::unique_ptr<nano::write_transaction_impl> write_transaction_impl) :
	impl (std::move (write_transaction_impl))
{
	/*
	 * For IO threads, we do not want them to block on creating write transactions.
	 */
	debug_assert (nano::thread_role::get () != nano::thread_role::name::io);
}

void * nano::write_transaction::get_handle () const
{
	return impl->get_handle ();
}

void nano::write_transaction::commit ()
{
	impl->commit ();
}

void nano::write_transaction::renew ()
{
	impl->renew ();
}

void nano::write_transaction::refresh ()
{
	impl->commit ();
	impl->renew ();
}

bool nano::write_transaction::contains (nano::tables table_a) const
{
	return impl->contains (table_a);
}

// clang-format off
nano::store::store (
	nano::block_store & block_store_a,
	nano::frontier_store & frontier_store_a,
	nano::account_store & account_store_a,
	nano::pending_store & pending_store_a,
	nano::unchecked_store & unchecked_store_a,
	nano::online_weight_store & online_weight_store_a,
	nano::pruned_store & pruned_store_a,
	nano::peer_store & peer_store_a,
	nano::confirmation_height_store & confirmation_height_store_a,
	nano::final_vote_store & final_vote_store_a,
	nano::version_store & version_store_a
) :
	block (block_store_a),
	frontier (frontier_store_a),
	account (account_store_a),
	pending (pending_store_a),
	unchecked (unchecked_store_a),
	online_weight (online_weight_store_a),
	pruned (pruned_store_a),
	peer (peer_store_a),
	confirmation_height (confirmation_height_store_a),
	final_vote (final_vote_store_a),
	version (version_store_a)
{
}
// clang-format on

auto nano::unchecked_store::equal_range (nano::transaction const & transaction, nano::block_hash const & dependency) -> std::pair<iterator, iterator>
{
	nano::unchecked_key begin_l{ dependency, 0 };
	nano::unchecked_key end_l{ nano::block_hash{ dependency.number () + 1 }, 0 };
	// Adjust for edge case where number () + 1 wraps around.
	auto end_iter = begin_l.previous < end_l.previous ? lower_bound (transaction, end_l) : end ();
	return std::make_pair (lower_bound (transaction, begin_l), std::move (end_iter));
}

auto nano::unchecked_store::full_range (nano::transaction const & transaction) -> std::pair<iterator, iterator>
{
	return std::make_pair (begin (transaction), end ());
}

std::vector<nano::unchecked_info> nano::unchecked_store::get (nano::transaction const & transaction, nano::block_hash const & dependency)
{
	auto range = equal_range (transaction, dependency);
	std::vector<nano::unchecked_info> result;
	auto & i = range.first;
	auto & n = range.second;
	for (; i != n; ++i)
	{
		auto const & key = i->first;
		auto const & value = i->second;
		debug_assert (key.hash == value.block->hash ());
		result.push_back (value);
	}
	return result;
}
