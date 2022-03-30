# Defines pretty printers for core railguard classes

import gdb.printing as printing


# ====== Vector<T> ======

class VectorPrinter:
    """Print a rg::Vector<T>"""

    class _iterator:
        def __init__(self, array, count):
            self.array = array
            self.i = 0
            self.count = count

        def __iter__(self):
            return self

        def __next__(self):
            return self.next()

        def next(self):
            if self.i == self.count:
                raise StopIteration

            value = self.array[self.i]
            self.i += 1

            return '[%d]' % (self.i - 1), value

    def __init__(self, val):
        self.val = val
        value_type = val.type.template_argument(0)

        # Get impl field
        impl = self.val['m_impl']

        # Get count field
        self.count = impl['m_count']

        # Get data field and cast it as a value_type*
        self.data = impl['m_data'].cast(value_type.pointer())

    def to_string(self):
        # Load all values of self.data
        values = [self.data[i] for i in range(self.count)]

        return "[" + ", ".join(map(str, values)) + "]"

    def children(self):
        return self._iterator(self.data, self.count)

    def display_hint(self):
        return 'array'

# ====== HashMap ======

class HashMapPrinter:
    """Prints a rg::HashMap"""

    class _iterator:
        def __init__(self, entries, capacity):
            self.entries = entries
            self.i = 0
            self.capacity = capacity

        def __iter__(self):
            return self

        def __next__(self):
            return self.next()

        def next(self):
            if self.i == self.capacity:
                raise StopIteration

            # Find the next entry that has no a null key
            while self.entries[self.i]['key'] == 0:
                self.i += 1
                if self.i == self.capacity:
                    raise StopIteration

            # Get the values of the entry
            self.key = self.entries[self.i]['key']
            self.value = self.entries[self.i]['value']

            self.i += 1

            return str(self.key), self.value


    def __init__(self, val):
        self.val = val

        # Get data
        data = self.val['m_data']

        # Get capacity and count
        self.capacity = data['capacity']
        self.count    = data['count']
        self.entries  = data['entries']

    def to_string(self):
        return 'HashMap(capacity=%d, count=%d)' % (self.capacity, self.count)

    def children(self):
        return self._iterator(self.entries, self.capacity)

    def display_hint(self):
        return 'array'

# ====== Register printers ======

def build_pretty_printer():
    pp = printing.RegexpCollectionPrettyPrinter("rg")
    pp.add_printer('Vector', '^rg::Vector<.*>$', VectorPrinter)
    pp.add_printer('HashMap', '^rg::HashMap$', HashMapPrinter)
    return pp


print("Registering pretty printers for railguard")
printing.register_pretty_printer(None, build_pretty_printer())
